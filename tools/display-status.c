#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void print_cf_string(CFStringRef value) {
    char buffer[512];

    if (value != NULL &&
        CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        fputs(buffer, stdout);
    } else {
        fputs("unknown", stdout);
    }
}

static CFStringRef copy_display_name(CGDirectDisplayID display) {
    io_service_t service = CGDisplayIOServicePort(display);
    CFDictionaryRef info;
    CFDictionaryRef names;
    CFStringRef result = NULL;
    CFIndex name_count;
    const void **keys = NULL;
    const void **values = NULL;

    if (service == MACH_PORT_NULL) {
        return NULL;
    }

    info = IODisplayCreateInfoDictionary(service, kIODisplayOnlyPreferredName);
    if (info == NULL) {
        return NULL;
    }

    names = CFDictionaryGetValue(info, CFSTR(kDisplayProductName));
    if (names != NULL && CFGetTypeID(names) == CFDictionaryGetTypeID()) {
        name_count = CFDictionaryGetCount(names);
        if (name_count > 0) {
            keys = calloc((size_t)name_count, sizeof(*keys));
            values = calloc((size_t)name_count, sizeof(*values));
        }
        if (keys != NULL && values != NULL) {
            CFDictionaryGetKeysAndValues(names, keys, values);
            for (CFIndex index = 0; index < name_count; ++index) {
                if (values[index] != NULL &&
                    CFGetTypeID(values[index]) == CFStringGetTypeID()) {
                    result = CFRetain(values[index]);
                    break;
                }
            }
        }
    }

    free(keys);
    free(values);
    CFRelease(info);
    return result;
}

int main(void) {
    CGDirectDisplayID displays[32];
    uint32_t count = 0;
    CGError status = CGGetOnlineDisplayList(32, displays, &count);

    if (status != kCGErrorSuccess) {
        fprintf(stderr, "CGGetOnlineDisplayList failed: %d\n", status);
        return 1;
    }

    printf("online_display_count=%u\n", count);
    for (uint32_t index = 0; index < count; ++index) {
        CGDirectDisplayID display = displays[index];
        CGRect bounds = CGDisplayBounds(display);
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display);
        CFStringRef name = copy_display_name(display);

        printf("display[%u] id=%u name=", index, display);
        print_cf_string(name);
        printf(" vendor=%u model=%u builtin=%s active=%s "
               "bounds=%.0fx%.0f@%.0f,%.0f",
               CGDisplayVendorNumber(display),
               CGDisplayModelNumber(display),
               CGDisplayIsBuiltin(display) ? "yes" : "no",
               CGDisplayIsActive(display) ? "yes" : "no",
               bounds.size.width,
               bounds.size.height,
               bounds.origin.x,
               bounds.origin.y);

        if (mode != NULL) {
            printf(" mode=%zux%zu@%.2f",
                   CGDisplayModeGetWidth(mode),
                   CGDisplayModeGetHeight(mode),
                   CGDisplayModeGetRefreshRate(mode));
            CGDisplayModeRelease(mode);
        }
        putchar('\n');

        if (name != NULL) {
            CFRelease(name);
        }
    }

    return 0;
}
