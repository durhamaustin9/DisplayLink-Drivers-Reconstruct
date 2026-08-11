#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "path_policy.h"

#include <dispatch/dispatch.h>
#include <errno.h>
#include <libproc.h>
#include <limits.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static NSString *const DLEngineBundleIdentifier = @"com.displaylink.DisplayLinkUserAgent";
static NSString *const DLControllerRelativePath =
    @"Contents/MacOS/DisplayLinkContainedController";
static NSString *const DLEngineRelativePath = @"Contents/Helpers/DisplayLink Core Engine.app";
static NSString *const DLMainRelativePath = @"Contents/MacOS/DisplayLinkUserAgent";

typedef struct {
    pid_t pid;
    uint64_t startSeconds;
    uint64_t startMicroseconds;
    BOOL bindingRequired;
    BOOL valid;
} DLProcessIdentity;

typedef NS_ENUM(NSInteger, DLBoundProcessState) {
    DLBoundProcessStateNotBound,
    DLBoundProcessStateGoneOrReused,
    DLBoundProcessStateSame,
    DLBoundProcessStateUncertain,
};

typedef NS_ENUM(NSInteger, DLProcessWaitResult) {
    DLProcessWaitResultUncertain,
    DLProcessWaitResultExited,
    DLProcessWaitResultRunning,
};

typedef NS_ENUM(NSInteger, DLZeroScanResult) {
    DLZeroScanResultUncertain,
    DLZeroScanResultBusy,
    DLZeroScanResultClear,
};

typedef NS_ENUM(NSInteger, DLForeignProcessScanResult) {
    DLForeignProcessScanResultUncertain,
    DLForeignProcessScanResultClear,
    DLForeignProcessScanResultFound,
};

typedef NS_ENUM(NSInteger, DLServiceRegistrationScanResult) {
    DLServiceRegistrationScanResultUncertain,
    DLServiceRegistrationScanResultClear,
    DLServiceRegistrationScanResultFound,
};

@interface DLCleanupResult : NSObject
@property(nonatomic) BOOL success;
@property(nonatomic, copy) NSString *message;
@end

@implementation DLCleanupResult
@end

static NSString *DLCanonicalExistingPath(NSString *path)
{
    if (path.length == 0) {
        return nil;
    }

    char resolved[PATH_MAX];
    if (realpath(path.fileSystemRepresentation, resolved) == NULL) {
        return nil;
    }
    return [[NSFileManager defaultManager]
        stringWithFileSystemRepresentation:resolved
                                  length:strlen(resolved)];
}

static NSString *DLProcessPath(pid_t pid)
{
    if (pid <= 0) {
        return nil;
    }

    char buffer[PROC_PIDPATHINFO_MAXSIZE];
    int length = proc_pidpath(pid, buffer, sizeof(buffer));
    if (length <= 0 || length >= (int)sizeof(buffer)) {
        return nil;
    }
    buffer[length] = '\0';
    NSString *reported = [[NSFileManager defaultManager]
        stringWithFileSystemRepresentation:buffer
                                  length:(NSUInteger)length];
    NSString *canonical = DLCanonicalExistingPath(reported);
    /* A running executable can be unlinked. Preserve the kernel-reported path so
       that condition cannot be mistaken for a zero-process state. */
    return canonical ?: reported.stringByStandardizingPath;
}

static BOOL DLPathIsExactMain(NSString *engineRoot, NSString *candidate)
{
    if (engineRoot.length == 0 || candidate.length == 0) {
        return NO;
    }
    return dl_path_is_exact_engine_executable(
        engineRoot.fileSystemRepresentation,
        candidate.fileSystemRepresentation);
}

static BOOL DLReadProcessIdentity(pid_t pid, DLProcessIdentity *identity)
{
    if (pid <= 0 || identity == NULL) {
        return NO;
    }
    struct proc_bsdinfo information = { 0 };
    int size = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0,
        &information, (int)sizeof(information));
    if (size != (int)sizeof(information)) {
        return NO;
    }
    identity->pid = pid;
    identity->startSeconds = information.pbi_start_tvsec;
    identity->startMicroseconds = information.pbi_start_tvusec;
    identity->bindingRequired = YES;
    identity->valid = YES;
    return YES;
}

static DLBoundProcessState DLStateOfBoundProcess(DLProcessIdentity identity)
{
    if (!identity.valid || identity.pid <= 0) {
        if (identity.bindingRequired) {
            return DLBoundProcessStateUncertain;
        }
        return DLBoundProcessStateNotBound;
    }

    DLProcessIdentity current = { 0 };
    if (DLReadProcessIdentity(identity.pid, &current)) {
        if (current.startSeconds == identity.startSeconds &&
            current.startMicroseconds == identity.startMicroseconds) {
            return DLBoundProcessStateSame;
        }
        return DLBoundProcessStateGoneOrReused;
    }

    errno = 0;
    if (kill(identity.pid, 0) == -1 && errno == ESRCH) {
        return DLBoundProcessStateGoneOrReused;
    }
    return DLBoundProcessStateUncertain;
}

static int DLRunTaskDiscardingOutput(NSString *executable, NSArray<NSString *> *arguments)
{
    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:executable];
    task.arguments = arguments;
    NSFileHandle *nullDevice = [NSFileHandle fileHandleWithNullDevice];
    task.standardOutput = nullDevice;
    task.standardError = nullDevice;

    NSError *error = nil;
    if (![task launchAndReturnError:&error]) {
        NSLog(@"DisplayLink controller: could not launch %@: %@", executable,
            error.localizedDescription);
        return -1;
    }
    [task waitUntilExit];
    return task.terminationStatus;
}

static BOOL DLVerifyCodeSignature(NSString *canonicalBundlePath, int *status)
{
    int result = DLRunTaskDiscardingOutput(@"/usr/bin/codesign",
        @[ @"--verify", @"--strict", @"--all-architectures", canonicalBundlePath ]);
    if (status != NULL) {
        *status = result;
    }
    return result == 0;
}

/* A nil result means enumeration was uncertain. */
static NSArray<NSNumber *> *DLAllProcessIdentifiers(void)
{
    int estimate = proc_listallpids(NULL, 0);
    if (estimate <= 0) {
        return nil;
    }

    size_t capacity = (size_t)estimate + 256U;
    const size_t maximumCapacity = (size_t)INT_MAX / sizeof(pid_t);
    if (capacity > maximumCapacity) {
        return nil;
    }

    for (NSUInteger attempt = 0; attempt < 6U; ++attempt) {
        pid_t *identifiers = calloc(capacity, sizeof(pid_t));
        if (identifiers == NULL) {
            return nil;
        }
        int count = proc_listallpids(identifiers, (int)(capacity * sizeof(pid_t)));
        if (count < 0) {
            free(identifiers);
            return nil;
        }
        if ((size_t)count >= capacity) {
            free(identifiers);
            if (capacity > maximumCapacity / 2U) {
                return nil;
            }
            capacity *= 2U;
            continue;
        }

        NSMutableArray<NSNumber *> *allIdentifiers = [NSMutableArray array];
        for (int index = 0; index < count; ++index) {
            pid_t pid = identifiers[index];
            if (pid > 0) {
                [allIdentifiers addObject:@(pid)];
            }
        }
        free(identifiers);
        return allIdentifiers;
    }
    return nil;
}

/* A nil result means enumeration was uncertain; an empty array means proven zero. */
static NSArray<NSNumber *> *DLEngineProcessIdentifiers(NSString *engineRoot)
{
    NSArray<NSNumber *> *allIdentifiers = DLAllProcessIdentifiers();
    if (allIdentifiers == nil) {
        return nil;
    }
    NSMutableArray<NSNumber *> *matches = [NSMutableArray array];
    for (NSNumber *number in allIdentifiers) {
        NSString *path = DLProcessPath(number.intValue);
        if (DLPathIsExactMain(engineRoot, path)) {
            [matches addObject:number];
        }
    }
    return matches;
}

static DLForeignProcessScanResult DLScanForForeignDisplayLinkProcesses(
    NSString *engineRoot, NSString *controllerExecutable, NSString **detail)
{
    NSArray<NSNumber *> *allIdentifiers = DLAllProcessIdentifiers();
    if (allIdentifiers == nil) {
        if (detail != NULL) {
            *detail = @"Process enumeration was uncertain while checking for foreign DisplayLink executables.";
        }
        return DLForeignProcessScanResultUncertain;
    }

    for (NSNumber *number in allIdentifiers) {
        pid_t pid = number.intValue;
        NSString *path = DLProcessPath(pid);
        if (path == nil) {
            errno = 0;
            if (kill(pid, 0) == -1 && errno == ESRCH) {
                continue;
            }
            if (detail != NULL) {
                *detail = [NSString stringWithFormat:
                    @"The executable path for live PID %d could not be resolved while "
                     "checking for foreign DisplayLink executables. No process was signaled.",
                    pid];
            }
            return DLForeignProcessScanResultUncertain;
        }

        if ([path isEqualToString:controllerExecutable] ||
            DLPathIsExactMain(engineRoot, path) ||
            !dl_path_is_known_displaylink_executable(
                path.fileSystemRepresentation)) {
            continue;
        }

        if (detail != NULL) {
            *detail = [NSString stringWithFormat:
                @"A foreign DisplayLink executable is running (PID %d, %@). Stop the "
                 "other DisplayLink build first; this controller will not adopt, stop, "
                 "or signal it.", pid, path];
        }
        return DLForeignProcessScanResultFound;
    }
    return DLForeignProcessScanResultClear;
}

static DLServiceRegistrationScanResult DLScanForForbiddenServiceRegistrations(
    NSString **detail)
{
    NSArray<NSString *> *labels = @[
        @"com.displaylink.XpcService",
        @"com.displaylink.CrashRestartHelper",
    ];
    NSString *domainPrefix = [NSString stringWithFormat:@"gui/%lu/",
        (unsigned long)getuid()];

    for (NSString *label in labels) {
        NSString *serviceTarget = [domainPrefix stringByAppendingString:label];
        int status = DLRunTaskDiscardingOutput(@"/bin/launchctl",
            @[ @"print", serviceTarget ]);
        if (status == 113) {
            continue;
        }
        if (status == 0) {
            if (detail != NULL) {
                *detail = [NSString stringWithFormat:
                    @"The foreign launchd service %@ is registered in the current GUI "
                     "domain. It may start after active processes disappear. This "
                     "controller only checks registration and will not unload or mutate it.",
                    label];
            }
            return DLServiceRegistrationScanResultFound;
        }
        if (detail != NULL) {
            *detail = [NSString stringWithFormat:
                @"The read-only launchd registration check for %@ returned status %d; "
                 "absence could not be proven and nothing was changed.", label, status];
        }
        return DLServiceRegistrationScanResultUncertain;
    }
    return DLServiceRegistrationScanResultClear;
}

static void DLSleepMilliseconds(unsigned milliseconds)
{
    struct timespec interval = {
        .tv_sec = (time_t)(milliseconds / 1000U),
        .tv_nsec = (long)(milliseconds % 1000U) * 1000000L,
    };
    while (nanosleep(&interval, &interval) == -1 && errno == EINTR) {
    }
}

static BOOL DLSignalExactMainProcesses(NSString *engineRoot, int signalNumber,
    DLProcessIdentity boundIdentity)
{
    NSArray<NSNumber *> *identifiers = DLEngineProcessIdentifiers(engineRoot);
    if (identifiers == nil) {
        return NO;
    }

    for (NSNumber *number in identifiers) {
        pid_t pid = number.intValue;
        NSString *revalidatedPath = DLProcessPath(pid);
        if (!DLPathIsExactMain(engineRoot, revalidatedPath)) {
            continue;
        }
        if (boundIdentity.valid && pid == boundIdentity.pid &&
            DLStateOfBoundProcess(boundIdentity) != DLBoundProcessStateSame) {
            NSLog(@"DisplayLink controller: PID %d was reused; it was not signaled", pid);
            continue;
        }
        if (kill(pid, signalNumber) == 0 || errno == ESRCH) {
            NSLog(@"DisplayLink controller: signal %d sent to exact Core PID %d (%@)",
                signalNumber, pid, revalidatedPath);
        } else {
            NSLog(@"DisplayLink controller: signal %d failed for exact Core PID %d: %s",
                signalNumber, pid, strerror(errno));
        }
    }
    return YES;
}

static DLProcessWaitResult DLCurrentProcessWaitResult(NSString *engineRoot,
    DLProcessIdentity boundIdentity, NSString **detail)
{
    DLBoundProcessState boundState = DLStateOfBoundProcess(boundIdentity);
    if (boundState == DLBoundProcessStateUncertain) {
        if (detail != NULL) {
            *detail = @"The lifetime identity of the tracked Core process could not be read.";
        }
        return DLProcessWaitResultUncertain;
    }
    if (boundState == DLBoundProcessStateSame) {
        NSString *boundPath = DLProcessPath(boundIdentity.pid);
        if (!DLPathIsExactMain(engineRoot, boundPath)) {
            if (detail != NULL) {
                *detail = [NSString stringWithFormat:
                    @"The tracked Core PID %d is still the same process, but its path is %@. "
                     "It will not be signaled as an exact nested executable.",
                    boundIdentity.pid, boundPath ?: @"unavailable"];
            }
            return DLProcessWaitResultUncertain;
        }
    }

    NSArray<NSNumber *> *identifiers = DLEngineProcessIdentifiers(engineRoot);
    if (identifiers == nil) {
        if (detail != NULL) {
            *detail = @"Process enumeration was uncertain.";
        }
        return DLProcessWaitResultUncertain;
    }
    if (boundState == DLBoundProcessStateSame || identifiers.count != 0) {
        if (detail != NULL) {
            *detail = [NSString stringWithFormat:
                @"The exact Core process is still running (PIDs %@).", identifiers];
        }
        return DLProcessWaitResultRunning;
    }
    return DLProcessWaitResultExited;
}

static DLProcessWaitResult DLWaitForExactProcessesToExit(NSString *engineRoot,
    DLProcessIdentity boundIdentity, unsigned timeoutMilliseconds, NSString **detail)
{
    unsigned elapsed = 0;
    while (elapsed < timeoutMilliseconds) {
        NSString *currentDetail = nil;
        DLProcessWaitResult state =
            DLCurrentProcessWaitResult(engineRoot, boundIdentity, &currentDetail);
        if (state != DLProcessWaitResultRunning) {
            if (detail != NULL) {
                *detail = currentDetail;
            }
            return state;
        }
        DLSleepMilliseconds(200U);
        elapsed += 200U;
    }
    return DLCurrentProcessWaitResult(engineRoot, boundIdentity, detail);
}

static DLZeroScanResult DLScanForZeroState(NSString *engineRoot,
    NSString *controllerExecutable, DLProcessIdentity boundIdentity, NSString **detail)
{
    DLProcessWaitResult state =
        DLCurrentProcessWaitResult(engineRoot, boundIdentity, detail);
    switch (state) {
        case DLProcessWaitResultUncertain:
            return DLZeroScanResultUncertain;
        case DLProcessWaitResultRunning:
            return DLZeroScanResultBusy;
        case DLProcessWaitResultExited: {
            NSString *foreignDetail = nil;
            DLForeignProcessScanResult foreignScan =
                DLScanForForeignDisplayLinkProcesses(
                    engineRoot, controllerExecutable, &foreignDetail);
            if (foreignScan == DLForeignProcessScanResultUncertain) {
                if (detail != NULL) {
                    *detail = foreignDetail ?: @"The foreign-process scan was uncertain.";
                }
                return DLZeroScanResultUncertain;
            }
            if (foreignScan == DLForeignProcessScanResultFound) {
                if (detail != NULL) {
                    *detail = foreignDetail ?: @"A foreign DisplayLink executable is running.";
                }
                return DLZeroScanResultBusy;
            }
            return DLZeroScanResultClear;
        }
    }
    if (detail != NULL) {
        *detail = @"The process state was invalid.";
    }
    return DLZeroScanResultUncertain;
}

static BOOL DLWaitForStableZeroState(NSString *engineRoot,
    NSString *controllerExecutable, DLProcessIdentity boundIdentity, NSString **failure)
{
    NSString *registrationDetail = nil;
    DLServiceRegistrationScanResult registrationScan =
        DLScanForForbiddenServiceRegistrations(&registrationDetail);
    if (registrationScan != DLServiceRegistrationScanResultClear) {
        if (failure != NULL) {
            *failure = registrationDetail ?:
                @"Foreign launchd service registration absence could not be proven.";
        }
        return NO;
    }

    NSUInteger consecutiveClearScans = 0;
    NSString *lastDetail = @"The zero-process state was not stable.";
    for (NSUInteger attempt = 0; attempt < 16U; ++attempt) {
        NSString *detail = nil;
        DLZeroScanResult scan =
            DLScanForZeroState(
                engineRoot, controllerExecutable, boundIdentity, &detail);
        if (scan == DLZeroScanResultUncertain) {
            if (failure != NULL) {
                *failure = detail ?: @"Cleanup verification was uncertain.";
            }
            return NO;
        }
        if (scan == DLZeroScanResultClear) {
            ++consecutiveClearScans;
            if (consecutiveClearScans >= 4U) {
                registrationDetail = nil;
                registrationScan =
                    DLScanForForbiddenServiceRegistrations(&registrationDetail);
                if (registrationScan != DLServiceRegistrationScanResultClear) {
                    if (failure != NULL) {
                        *failure = registrationDetail ?:
                            @"The final launchd registration check was uncertain.";
                    }
                    return NO;
                }
                return YES;
            }
        } else {
            consecutiveClearScans = 0;
            lastDetail = detail ?: lastDetail;
        }
        DLSleepMilliseconds(500U);
    }
    if (failure != NULL) {
        *failure = lastDetail;
    }
    return NO;
}

static DLCleanupResult *DLPerformCleanup(NSString *engineRoot,
    NSString *controllerExecutable, NSString *mainExecutable,
    DLProcessIdentity boundIdentity)
{
    DLCleanupResult *result = [[DLCleanupResult alloc] init];

    /* Ask only the exact nested application to take its normal termination path. */
    dispatch_sync(dispatch_get_main_queue(), ^{
        for (NSRunningApplication *application in
             [NSWorkspace sharedWorkspace].runningApplications) {
            if (![application.bundleIdentifier isEqualToString:DLEngineBundleIdentifier]) {
                continue;
            }
            NSString *path = DLProcessPath(application.processIdentifier);
            if ([path isEqualToString:mainExecutable] &&
                DLPathIsExactMain(engineRoot, path)) {
                if (boundIdentity.valid &&
                    application.processIdentifier == boundIdentity.pid &&
                    DLStateOfBoundProcess(boundIdentity) != DLBoundProcessStateSame) {
                    continue;
                }
                [application terminate];
            }
        }
    });
    DLSleepMilliseconds(600U);

    if (!DLSignalExactMainProcesses(engineRoot, SIGTERM, boundIdentity)) {
        result.success = NO;
        result.message = @"Process enumeration failed, so no signal was sent and zero could not be proven.";
        return result;
    }

    NSString *failure = nil;
    DLProcessWaitResult waitResult =
        DLWaitForExactProcessesToExit(engineRoot, boundIdentity, 2400U, &failure);
    if (waitResult == DLProcessWaitResultUncertain) {
        result.success = NO;
        result.message = failure ?: @"The tracked process identity was uncertain after SIGTERM.";
        return result;
    }
    if (waitResult == DLProcessWaitResultRunning &&
        !DLSignalExactMainProcesses(engineRoot, SIGKILL, boundIdentity)) {
        result.success = NO;
        result.message = @"Process enumeration failed before exact-path SIGKILL escalation.";
        return result;
    }

    failure = nil;
    if (!DLWaitForStableZeroState(
            engineRoot, controllerExecutable, boundIdentity, &failure)) {
        result.success = NO;
        result.message = failure ?: @"Cleanup could not prove a stable zero-process state.";
        return result;
    }

    result.success = YES;
    result.message = @"The bound Core process is gone, known foreign DisplayLink "
        "executable paths are absent, and pinned foreign launchd services are unregistered.";
    return result;
}

@interface DLControllerDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, copy) NSString *outerRoot;
@property(nonatomic, copy) NSString *controllerExecutable;
@property(nonatomic, copy) NSString *engineRoot;
@property(nonatomic, copy) NSString *mainExecutable;
@property(nonatomic, strong) NSRunningApplication *engineApplication;
@property(nonatomic) pid_t enginePID;
@property(nonatomic) DLProcessIdentity engineIdentity;
@property(nonatomic, strong) NSTimer *mainMonitorTimer;
@property(nonatomic) BOOL launchInFlight;
@property(nonatomic) BOOL shutdownRequested;
@property(nonatomic) BOOL cleanupInProgress;
@property(nonatomic) BOOL cleanupSucceeded;
@property(nonatomic) BOOL cleanupFailed;
@property(nonatomic) BOOL terminateReplyPending;
@property(nonatomic, copy) NSString *fatalAfterCleanup;
@property(nonatomic, strong) dispatch_source_t termSignalSource;
@property(nonatomic, strong) dispatch_source_t interruptSignalSource;
@end

@implementation DLControllerDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

    self.outerRoot = DLCanonicalExistingPath(NSBundle.mainBundle.bundlePath);
    if (self.outerRoot == nil) {
        [self showStartupErrorAndExit:@"The controller app path could not be canonicalized."];
        return;
    }

    int signatureStatus = -1;
    if (!DLVerifyCodeSignature(self.outerRoot, &signatureStatus)) {
        [self showStartupErrorAndExit:[NSString stringWithFormat:
            @"The canonical outer controller bundle failed code-signature verification (status %d).",
            signatureStatus]];
        return;
    }

    NSString *plannedController =
        [self.outerRoot stringByAppendingPathComponent:DLControllerRelativePath];
    self.controllerExecutable = DLCanonicalExistingPath(plannedController);
    if (self.controllerExecutable == nil ||
        ![self.controllerExecutable isEqualToString:plannedController]) {
        [self showStartupErrorAndExit:
            @"The controller executable is missing, moved, or linked outside its app."];
        return;
    }

    NSString *plannedEngine = [self.outerRoot stringByAppendingPathComponent:DLEngineRelativePath];
    self.engineRoot = DLCanonicalExistingPath(plannedEngine);
    if (self.engineRoot == nil || ![self.engineRoot isEqualToString:plannedEngine]) {
        [self showStartupErrorAndExit:
            @"The embedded Core engine is missing, moved, or linked outside the controller app."];
        return;
    }
    if (!DLVerifyCodeSignature(self.engineRoot, &signatureStatus)) {
        [self showStartupErrorAndExit:[NSString stringWithFormat:
            @"The embedded Core engine failed code-signature verification (status %d).",
            signatureStatus]];
        return;
    }

    NSString *plannedMain = [self.engineRoot stringByAppendingPathComponent:DLMainRelativePath];
    self.mainExecutable = DLCanonicalExistingPath(plannedMain);
    if (self.mainExecutable == nil ||
        ![self.mainExecutable isEqualToString:plannedMain] ||
        !DLPathIsExactMain(self.engineRoot, self.mainExecutable)) {
        [self showStartupErrorAndExit:
            @"The embedded Core executable failed the exact-main path policy."];
        return;
    }

    NSString *foreignFailure = nil;
    DLForeignProcessScanResult foreignScan =
        DLScanForForeignDisplayLinkProcesses(
            self.engineRoot, self.controllerExecutable, &foreignFailure);
    if (foreignScan != DLForeignProcessScanResultClear) {
        [self showStartupErrorAndExit:foreignFailure ?:
            @"Foreign DisplayLink process absence could not be proven before launch."];
        return;
    }

    NSString *registrationFailure = nil;
    DLServiceRegistrationScanResult registrationScan =
        DLScanForForbiddenServiceRegistrations(&registrationFailure);
    if (registrationScan != DLServiceRegistrationScanResultClear) {
        [self showStartupErrorAndExit:registrationFailure ?:
            @"Foreign launchd service registration absence could not be proven before launch."];
        return;
    }

    [self installSignalHandlers];
    NSNotificationCenter *workspaceNotifications =
        [NSWorkspace sharedWorkspace].notificationCenter;
    [workspaceNotifications addObserver:self
                                selector:@selector(workspaceApplicationTerminated:)
                                    name:NSWorkspaceDidTerminateApplicationNotification
                                  object:nil];
    [workspaceNotifications addObserver:self
                                selector:@selector(workspaceApplicationLaunched:)
                                    name:NSWorkspaceDidLaunchApplicationNotification
                                  object:nil];

    NSString *applicationFailure = nil;
    NSArray<NSRunningApplication *> *exactApplications =
        [self exactRunningDisplayLinkApplicationsOrSetFailure:&applicationFailure];
    if (exactApplications == nil) {
        [self showStartupErrorAndExit:applicationFailure];
        return;
    }

    NSArray<NSNumber *> *exactPIDs = DLEngineProcessIdentifiers(self.engineRoot);
    if (exactPIDs == nil) {
        [self showStartupErrorAndExit:
            @"Process enumeration was uncertain before launch; nothing was changed."];
        return;
    }
    if (exactApplications.count > 1U || exactPIDs.count > 1U) {
        [self showStartupErrorAndExit:
            @"More than one exact Core main process exists; the controller will not choose one."];
        return;
    }
    if (exactApplications.count == 1U) {
        NSRunningApplication *existing = exactApplications.firstObject;
        if (exactPIDs.count != 1U ||
            exactPIDs.firstObject.intValue != existing.processIdentifier) {
            [self showStartupErrorAndExit:
                @"The exact Core process could not be bound to one live macOS application."];
            return;
        }
        DLProcessIdentity existingIdentity = {
            .pid = existing.processIdentifier,
            .bindingRequired = YES,
        };
        if (!DLReadProcessIdentity(existing.processIdentifier, &existingIdentity) ||
            DLStateOfBoundProcess(existingIdentity) != DLBoundProcessStateSame ||
            !DLPathIsExactMain(self.engineRoot,
                DLProcessPath(existing.processIdentifier))) {
            [self showStartupErrorAndExit:
                @"The existing exact Core process could not be bound to a stable lifetime token."];
            return;
        }
        self.engineIdentity = existingIdentity;
        [self adoptValidatedApplication:existing];
        return;
    }
    if (exactPIDs.count != 0U) {
        [self showStartupErrorAndExit:
            @"An exact Core executable is running without a matching macOS application; nothing was changed."];
        return;
    }

    [self launchNestedEngine];
}

- (void)installSignalHandlers
{
    (void)signal(SIGTERM, SIG_IGN);
    (void)signal(SIGINT, SIG_IGN);

    __weak DLControllerDelegate *weakSelf = self;
    self.termSignalSource = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_SIGNAL, (uintptr_t)SIGTERM, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(self.termSignalSource, ^{
        [weakSelf requestCleanup:@"controller received SIGTERM"];
    });
    dispatch_resume(self.termSignalSource);

    self.interruptSignalSource = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_SIGNAL, (uintptr_t)SIGINT, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(self.interruptSignalSource, ^{
        [weakSelf requestCleanup:@"controller received SIGINT"];
    });
    dispatch_resume(self.interruptSignalSource);
}

- (NSArray<NSRunningApplication *> *)exactRunningDisplayLinkApplicationsOrSetFailure:
    (NSString **)failure
{
    NSMutableArray<NSRunningApplication *> *exact = [NSMutableArray array];
    for (NSRunningApplication *application in
         [NSWorkspace sharedWorkspace].runningApplications) {
        if (application.isTerminated ||
            ![application.bundleIdentifier isEqualToString:DLEngineBundleIdentifier]) {
            continue;
        }
        NSString *path = DLProcessPath(application.processIdentifier);
        if (![path isEqualToString:self.mainExecutable] ||
            !DLPathIsExactMain(self.engineRoot, path)) {
            if (failure != NULL) {
                *failure = [NSString stringWithFormat:
                    @"A foreign DisplayLink main application is already running (PID %d, %@). "
                     "This controller will not adopt or stop it.",
                    application.processIdentifier, path ?: @"path unavailable"];
            }
            return nil;
        }
        [exact addObject:application];
    }
    return exact;
}

- (void)launchNestedEngine
{
    self.launchInFlight = YES;
    NSURL *engineURL = [NSURL fileURLWithPath:self.engineRoot isDirectory:YES];
    NSWorkspaceOpenConfiguration *configuration =
        [NSWorkspaceOpenConfiguration configuration];
    configuration.activates = YES;
    configuration.arguments = @[
        @"-AppAutostart", @"false",
        @"-ShowMenuBarIcon", @"true",
        @"-DisableRestartOnCrash", @"true",
        @"-FirstSetupCompleted", @"true"
    ];

    __weak DLControllerDelegate *weakSelf = self;
    [[NSWorkspace sharedWorkspace]
        openApplicationAtURL:engineURL
               configuration:configuration
           completionHandler:^(NSRunningApplication *application, NSError *error) {
               dispatch_async(dispatch_get_main_queue(), ^{
                   [weakSelf nestedEngineOpenCompleted:application error:error];
               });
           }];
}

- (void)nestedEngineOpenCompleted:(NSRunningApplication *)application
                            error:(NSError *)error
{
    self.launchInFlight = NO;

    /* Bind a live exact result for cleanup even when shutdown won the race. This
       records only its immutable lifetime token; it does not adopt or activate it. */
    BOOL returnedApplicationIsLive = application != nil &&
        !application.isTerminated && application.processIdentifier > 0;
    NSString *returnedPath = returnedApplicationIsLive ?
        DLProcessPath(application.processIdentifier) : nil;
    BOOL returnedApplicationIsExact = returnedApplicationIsLive &&
        [application.bundleIdentifier isEqualToString:DLEngineBundleIdentifier] &&
        [returnedPath isEqualToString:self.mainExecutable] &&
        DLPathIsExactMain(self.engineRoot, returnedPath);
    NSString *returnedIdentityFailure = nil;
    if (returnedApplicationIsExact) {
        DLProcessIdentity launchIdentity = {
            .pid = application.processIdentifier,
            .bindingRequired = YES,
        };
        if (DLReadProcessIdentity(application.processIdentifier, &launchIdentity) &&
            DLStateOfBoundProcess(launchIdentity) == DLBoundProcessStateSame &&
            [DLProcessPath(application.processIdentifier) isEqualToString:self.mainExecutable]) {
            self.engineIdentity = launchIdentity;
        } else {
            self.engineIdentity = launchIdentity;
            returnedIdentityFailure =
                @"The launched exact Core process could not be bound to a stable lifetime token.";
        }
    } else if (returnedApplicationIsLive) {
        returnedIdentityFailure = [NSString stringWithFormat:
            @"macOS returned a foreign application during Core launch (PID %d, %@). "
             "It will not be adopted or touched.",
            application.processIdentifier, returnedPath ?: @"path unavailable"];
    }

    /* A shutdown request made during NSWorkspace open always wins. The completion
       may identify a process, but it is never adopted after that state transition. */
    if (self.shutdownRequested) {
        if (returnedIdentityFailure != nil) {
            self.fatalAfterCleanup = returnedIdentityFailure;
        }
        [self beginCleanupIfReady];
        return;
    }
    if (error != nil || application == nil) {
        self.fatalAfterCleanup = [NSString stringWithFormat:
            @"macOS could not open the embedded Core engine.\n\n%@",
            error.localizedDescription ?: @"No application was returned."];
        [self requestCleanup:@"nested Core launch failed"];
        return;
    }

    NSString *validationFailure = nil;
    NSArray<NSRunningApplication *> *exactApplications =
        [self exactRunningDisplayLinkApplicationsOrSetFailure:&validationFailure];
    NSString *path = DLProcessPath(application.processIdentifier);
    NSArray<NSNumber *> *exactPIDs = DLEngineProcessIdentifiers(self.engineRoot);
    returnedApplicationIsLive = !application.isTerminated &&
        application.processIdentifier > 0 &&
        [application.bundleIdentifier isEqualToString:DLEngineBundleIdentifier] &&
        [path isEqualToString:self.mainExecutable] &&
        DLPathIsExactMain(self.engineRoot, path);
    BOOL oneExactBinding = exactApplications != nil && exactApplications.count == 1U &&
        exactApplications.firstObject.processIdentifier == application.processIdentifier &&
        exactPIDs != nil && exactPIDs.count == 1U &&
        exactPIDs.firstObject.intValue == application.processIdentifier;
    NSString *foreignFailure = nil;
    DLForeignProcessScanResult foreignScan =
        DLScanForForeignDisplayLinkProcesses(
            self.engineRoot, self.controllerExecutable, &foreignFailure);
    BOOL foreignProcessesAbsent =
        (foreignScan == DLForeignProcessScanResultClear);

    if (!returnedApplicationIsLive || !oneExactBinding || !foreignProcessesAbsent) {
        self.fatalAfterCleanup = validationFailure ?: foreignFailure ?:
            returnedIdentityFailure ?:
            @"The launched process could not be bound to exactly one live Core main executable.";
        [self requestCleanup:@"Core launch identity validation failed"];
        return;
    }
    [self adoptValidatedApplication:application];
}

- (void)adoptValidatedApplication:(NSRunningApplication *)application
{
    if (self.shutdownRequested) {
        [self beginCleanupIfReady];
        return;
    }
    NSString *path = DLProcessPath(application.processIdentifier);
    BOOL exactCandidate = !application.isTerminated && application.processIdentifier > 0 &&
        [application.bundleIdentifier isEqualToString:DLEngineBundleIdentifier] &&
        [path isEqualToString:self.mainExecutable] &&
        DLPathIsExactMain(self.engineRoot, path);
    DLProcessIdentity identity = {
        .pid = application.processIdentifier,
        .bindingRequired = exactCandidate,
    };
    BOOL identityRead = exactCandidate &&
        DLReadProcessIdentity(application.processIdentifier, &identity);
    if (!exactCandidate || !identityRead) {
        if (exactCandidate) {
            self.engineIdentity = identity;
        }
        self.fatalAfterCleanup =
            @"The Core process exited or changed identity immediately before adoption.";
        [self requestCleanup:@"Core process failed final live-PID validation"];
        return;
    }
    self.engineIdentity = identity;

    NSString *finalPath = DLProcessPath(application.processIdentifier);
    if (application.isTerminated ||
        DLStateOfBoundProcess(identity) != DLBoundProcessStateSame ||
        ![finalPath isEqualToString:self.mainExecutable] ||
        !DLPathIsExactMain(self.engineRoot, finalPath)) {
        self.fatalAfterCleanup =
            @"The Core process failed the final lifetime-token and path revalidation.";
        [self requestCleanup:@"Core process changed during adoption"];
        return;
    }

    self.engineApplication = application;
    self.enginePID = application.processIdentifier;
    self.engineIdentity = identity;
    NSLog(@"DisplayLink controller: supervising one exact Core PID %d at %@",
        self.enginePID, self.mainExecutable);
    self.mainMonitorTimer = [NSTimer scheduledTimerWithTimeInterval:1.0
                                                             target:self
                                                           selector:@selector(monitorTrackedMain)
                                                           userInfo:nil
                                                            repeats:YES];
    [application activateWithOptions:NSApplicationActivateIgnoringOtherApps];
}

- (void)monitorTrackedMain
{
    if (self.shutdownRequested || self.cleanupInProgress || self.cleanupSucceeded) {
        return;
    }

    NSString *foreignFailure = nil;
    DLForeignProcessScanResult foreignScan =
        DLScanForForeignDisplayLinkProcesses(
            self.engineRoot, self.controllerExecutable, &foreignFailure);
    if (foreignScan != DLForeignProcessScanResultClear) {
        self.fatalAfterCleanup = foreignFailure ?:
            @"Continuous monitoring could not prove foreign DisplayLink process absence.";
        [self requestCleanup:@"foreign or uncertain DisplayLink process appeared"];
        return;
    }

    NSString *path = self.enginePID > 0 ? DLProcessPath(self.enginePID) : nil;
    if (self.engineApplication == nil || self.engineApplication.isTerminated ||
        self.enginePID <= 0 ||
        DLStateOfBoundProcess(self.engineIdentity) != DLBoundProcessStateSame ||
        ![path isEqualToString:self.mainExecutable] ||
        !DLPathIsExactMain(self.engineRoot, path)) {
        [self requestCleanup:@"tracked Core main exited or changed identity"];
    }
}

- (void)workspaceApplicationLaunched:(NSNotification *)notification
{
    NSRunningApplication *application = notification.userInfo[NSWorkspaceApplicationKey];
    if (application == nil ||
        ![application.bundleIdentifier isEqualToString:DLEngineBundleIdentifier]) {
        return;
    }

    NSString *path = DLProcessPath(application.processIdentifier);
    /* NSWorkspace may publish the launch before proc_pidpath is readable. The open
       completion performs the full global/path/token validation for this window. */
    if (self.launchInFlight) {
        return;
    }
    if (application.processIdentifier == self.enginePID &&
        [path isEqualToString:self.mainExecutable]) {
        return;
    }

    self.fatalAfterCleanup = [NSString stringWithFormat:
        @"Another DisplayLink main application appeared while the controller was active "
         "(PID %d, %@). It will not be touched.",
        application.processIdentifier, path ?: @"path unavailable"];
    [self requestCleanup:@"unexpected DisplayLink main appeared"];
}

- (void)workspaceApplicationTerminated:(NSNotification *)notification
{
    NSRunningApplication *application = notification.userInfo[NSWorkspaceApplicationKey];
    if (application != nil && self.enginePID > 0 &&
        application.processIdentifier == self.enginePID) {
        [self requestCleanup:@"tracked Core main exited"];
    }
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender
                    hasVisibleWindows:(BOOL)flag
{
    (void)sender;
    (void)flag;
    if (self.launchInFlight || self.cleanupInProgress || self.cleanupSucceeded) {
        return NO;
    }
    if (self.cleanupFailed) {
        [self requestCleanup:@"application reopened after incomplete cleanup"];
        return NO;
    }
    NSString *path = self.enginePID > 0 ? DLProcessPath(self.enginePID) : nil;
    if (!self.shutdownRequested && !self.engineApplication.isTerminated &&
        DLStateOfBoundProcess(self.engineIdentity) == DLBoundProcessStateSame &&
        [path isEqualToString:self.mainExecutable] &&
        DLPathIsExactMain(self.engineRoot, path)) {
        [self.engineApplication activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        return YES;
    }
    [self requestCleanup:@"reopen found no validated Core main"];
    return NO;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    (void)sender;
    if (self.cleanupSucceeded || self.engineRoot.length == 0) {
        return NSTerminateNow;
    }
    self.terminateReplyPending = YES;
    [self requestCleanup:@"normal controller termination requested"];
    return NSTerminateLater;
}

- (void)requestCleanup:(NSString *)reason
{
    if (![NSThread isMainThread]) {
        __weak DLControllerDelegate *weakSelf = self;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf requestCleanup:reason];
        });
        return;
    }
    if (self.cleanupSucceeded || self.cleanupInProgress || self.engineRoot.length == 0) {
        return;
    }

    self.shutdownRequested = YES;
    self.cleanupFailed = NO;
    [self.mainMonitorTimer invalidate];
    self.mainMonitorTimer = nil;
    NSLog(@"DisplayLink controller: cleanup requested (%@)%@", reason,
        self.launchInFlight ? @"; waiting for launch completion" : @"");
    [self beginCleanupIfReady];
}

- (void)beginCleanupIfReady
{
    if (self.launchInFlight || self.cleanupInProgress || self.cleanupSucceeded ||
        !self.shutdownRequested || self.engineRoot.length == 0) {
        return;
    }
    self.cleanupInProgress = YES;
    self.cleanupFailed = NO;

    NSString *engineRoot = [self.engineRoot copy];
    NSString *controllerExecutable = [self.controllerExecutable copy];
    NSString *mainExecutable = [self.mainExecutable copy];
    DLProcessIdentity boundIdentity = self.engineIdentity;
    __weak DLControllerDelegate *weakSelf = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        DLCleanupResult *result =
            DLPerformCleanup(
                engineRoot, controllerExecutable, mainExecutable, boundIdentity);
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf cleanupCompleted:result];
        });
    });
}

- (void)cleanupCompleted:(DLCleanupResult *)result
{
    self.cleanupInProgress = NO;
    if (result.success) {
        self.cleanupSucceeded = YES;
        NSLog(@"DisplayLink controller: stable Core zero-process state verified");
        if (self.fatalAfterCleanup.length != 0) {
            NSString *message = self.fatalAfterCleanup;
            self.fatalAfterCleanup = nil;
            [self showAlertWithTitle:@"DisplayLink Contained Could Not Continue"
                             message:message
                              button:@"Close"];
            exit(EXIT_FAILURE);
        }
        if (self.terminateReplyPending) {
            [NSApp replyToApplicationShouldTerminate:YES];
        } else {
            [NSApp terminate:nil];
        }
        return;
    }

    self.cleanupFailed = YES;
    NSString *message = [NSString stringWithFormat:
        @"The controller stayed open because it could not prove that the exact Core process "
         "and its bound lifetime identity stopped, that known foreign DisplayLink executable "
         "paths were absent, and that pinned foreign launchd services were unregistered. "
         "It never signals or unloads a foreign process or service.\n\n%@",
        result.message ?: @"No diagnostic was returned."];
    [NSApp activateIgnoringOtherApps:YES];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = @"Cleanup Incomplete";
    alert.informativeText = message;
    [alert addButtonWithTitle:@"Retry Cleanup"];
    [alert addButtonWithTitle:@"Keep Controller Open"];
    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        [self requestCleanup:@"user requested cleanup retry"];
    } else if (self.terminateReplyPending) {
        self.terminateReplyPending = NO;
        [NSApp replyToApplicationShouldTerminate:NO];
    }
}

- (void)showStartupErrorAndExit:(NSString *)message
{
    [self showAlertWithTitle:@"DisplayLink Contained Did Not Start"
                     message:message
                      button:@"Close"];
    exit(EXIT_FAILURE);
}

- (void)showAlertWithTitle:(NSString *)title
                   message:(NSString *)message
                    button:(NSString *)button
{
    [NSApp activateIgnoringOtherApps:YES];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = title;
    alert.informativeText = message;
    [alert addButtonWithTitle:button];
    [alert runModal];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    (void)notification;
    [self.mainMonitorTimer invalidate];
    [[NSWorkspace sharedWorkspace].notificationCenter removeObserver:self];
}

@end


int main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication *application = [NSApplication sharedApplication];
        DLControllerDelegate *delegate = [[DLControllerDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }
    return EXIT_SUCCESS;
}
