#import <Foundation/Foundation.h>
#import <IOUSBHost/IOUSBHost.h>

#include "descriptor_model.h"
#include "probe_model.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int
read_u32(io_registry_entry_t entry, CFStringRef key, uint32_t *value)
{
    CFTypeRef property = IORegistryEntryCreateCFProperty(entry, key,
        kCFAllocatorDefault, 0);
    int64_t number = 0;
    int found = 0;

    if (property != NULL && CFGetTypeID(property) == CFNumberGetTypeID() &&
        CFNumberGetValue((CFNumberRef)property, kCFNumberSInt64Type, &number) &&
        number >= 0 && number <= UINT32_MAX) {
        *value = (uint32_t)number;
        found = 1;
    }
    if (property != NULL) {
        CFRelease(property);
    }
    return found;
}

static int
entry_is_target(io_registry_entry_t entry)
{
    uint32_t vendor = 0;
    uint32_t product = 0;
    return read_u32(entry, CFSTR("idVendor"), &vendor) &&
        read_u32(entry, CFSTR("idProduct"), &product) &&
        db_probe_matches_device(vendor, product);
}

static int
interface_belongs_to_target(io_registry_entry_t interface)
{
    io_registry_entry_t current = interface;
    io_registry_entry_t parent = IO_OBJECT_NULL;
    int owns_current = 0;
    int result = 0;

    while (IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent) ==
        KERN_SUCCESS) {
        if (owns_current) {
            IOObjectRelease(current);
        }
        current = parent;
        owns_current = 1;
        parent = IO_OBJECT_NULL;
        if (entry_is_target(current)) {
            result = 1;
            break;
        }
    }
    if (owns_current) {
        IOObjectRelease(current);
    }
    return result;
}

static int
entry_has_owner(io_registry_entry_t entry)
{
    CFTypeRef owner = IORegistryEntryCreateCFProperty(entry,
        CFSTR("UsbExclusiveOwner"), kCFAllocatorDefault, 0);
    int occupied = 0;
    if (owner != NULL) {
        if (CFGetTypeID(owner) == CFStringGetTypeID()) {
            occupied = CFStringGetLength((CFStringRef)owner) > 0;
        } else {
            occupied = 1;
        }
        CFRelease(owner);
    }
    return occupied;
}

static int
candidate_interfaces_are_unowned(void)
{
    io_iterator_t iterator = IO_OBJECT_NULL;
    io_registry_entry_t entry = IO_OBJECT_NULL;
    CFMutableDictionaryRef match = IOServiceMatching("IOUSBHostInterface");
    int target_count = 0;
    int occupied = 0;

    if (match == NULL || IOServiceGetMatchingServices(kIOMainPortDefault,
        match, &iterator) != KERN_SUCCESS) {
        fputs("descriptor-reader: could not enumerate USB interfaces\n", stderr);
        return -1;
    }

    while ((entry = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        uint32_t number = UINT32_MAX;
        if (interface_belongs_to_target(entry) &&
            read_u32(entry, CFSTR("bInterfaceNumber"), &number) &&
            (number == 0 || number == 1)) {
            ++target_count;
            if (entry_has_owner(entry)) {
                occupied = 1;
            }
        }
        IOObjectRelease(entry);
    }
    IOObjectRelease(iterator);

    if (target_count != 2) {
        fprintf(stderr, "descriptor-reader: expected two candidate interfaces; found %d\n",
            target_count);
        return -1;
    }
    return occupied ? 0 : 1;
}

static io_service_t
copy_single_target_device(uint32_t *revision, uint32_t *speed)
{
    io_iterator_t iterator = IO_OBJECT_NULL;
    io_service_t entry = IO_OBJECT_NULL;
    io_service_t target = IO_OBJECT_NULL;
    CFMutableDictionaryRef match = IOServiceMatching("IOUSBHostDevice");

    if (match == NULL || IOServiceGetMatchingServices(kIOMainPortDefault,
        match, &iterator) != KERN_SUCCESS) {
        return IO_OBJECT_NULL;
    }

    while ((entry = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        if (!entry_is_target(entry)) {
            IOObjectRelease(entry);
            continue;
        }
        if (target != IO_OBJECT_NULL) {
            IOObjectRelease(entry);
            IOObjectRelease(target);
            target = IO_OBJECT_NULL;
            break;
        }
        target = entry;
    }
    IOObjectRelease(iterator);

    if (target != IO_OBJECT_NULL &&
        (!read_u32(target, CFSTR("bcdDevice"), revision) ||
            !read_u32(target, CFSTR("USBSpeed"), speed))) {
        IOObjectRelease(target);
        return IO_OBJECT_NULL;
    }
    return target;
}

static const IOUSBSuperSpeedEndpointCompanionDescriptor *
companion_for_endpoint(const IOUSBConfigurationDescriptor *configuration,
    const IOUSBEndpointDescriptor *endpoint)
{
    const IOUSBDescriptorHeader *found =
        IOUSBGetNextAssociatedDescriptorWithType(configuration,
            (const IOUSBDescriptorHeader *)endpoint, NULL,
            kIOUSBDescriptorTypeSuperSpeedUSBEndpointCompanion);
    return (const IOUSBSuperSpeedEndpointCompanionDescriptor *)found;
}

static int
print_candidate_descriptors(const IOUSBConfigurationDescriptor *configuration,
    uint32_t speed)
{
    const IOUSBInterfaceDescriptor *interface = NULL;
    size_t interface_count = 0;
    size_t endpoint_count = 0;

    while ((interface = IOUSBGetNextInterfaceDescriptor(configuration,
        (const IOUSBDescriptorHeader *)interface)) != NULL) {
        if (interface->bInterfaceNumber != 0 &&
            interface->bInterfaceNumber != 1) {
            continue;
        }
        ++interface_count;
        printf("interface %u alt=%u class=%02x/%02x/%02x endpoints=%u\n",
            interface->bInterfaceNumber, interface->bAlternateSetting,
            interface->bInterfaceClass, interface->bInterfaceSubClass,
            interface->bInterfaceProtocol, interface->bNumEndpoints);

        const IOUSBEndpointDescriptor *endpoint = NULL;
        size_t parsed_interface_endpoints = 0;
        while ((endpoint = IOUSBGetNextEndpointDescriptor(configuration,
            interface, (const IOUSBDescriptorHeader *)endpoint)) != NULL) {
            const IOUSBSuperSpeedEndpointCompanionDescriptor *companion =
                companion_for_endpoint(configuration, endpoint);
            DBEndpointFacts facts = {0};
            if (!db_endpoint_facts(speed, endpoint, companion, &facts)) {
                fputs("descriptor-reader: invalid endpoint descriptor\n", stderr);
                return 0;
            }
            printf("  endpoint 0x%02x direction=%s type=%s max-packet=%u "
                "interval=%u microframes=%u frames=%u",
                facts.address, db_endpoint_direction_name(facts.direction),
                db_endpoint_transfer_type_name(facts.transfer_type),
                facts.maximum_packet_size, facts.raw_interval,
                facts.interval_microframes, facts.interval_frames);
            if (facts.has_superspeed_companion) {
                printf(" max-burst-packets=%u burst-bytes=%u max-streams=%u",
                    facts.maximum_burst_packets, facts.burst_size_bytes,
                    facts.maximum_streams);
            }
            putchar('\n');
            ++endpoint_count;
            ++parsed_interface_endpoints;
        }
        if (parsed_interface_endpoints != interface->bNumEndpoints) {
            fprintf(stderr, "descriptor-reader: interface %u declares %u "
                "endpoints but %zu were parsed\n", interface->bInterfaceNumber,
                interface->bNumEndpoints, parsed_interface_endpoints);
            return 0;
        }
    }

    if (interface_count != 2 || endpoint_count == 0) {
        fprintf(stderr, "descriptor-reader: expected two candidate interfaces "
            "and at least one endpoint; found %zu and %zu\n",
            interface_count, endpoint_count);
        return 0;
    }
    return 1;
}

static void
usage(const char *program)
{
    fprintf(stderr,
        "usage: %s --read-standard-descriptors\n\n"
        "The opt-in flag permits only standard USB configuration-descriptor "
        "reads from 17e9:4323.\n",
        program);
}

int
main(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "--read-standard-descriptors") != 0) {
        usage(argv[0]);
        return 64;
    }

    @autoreleasepool {
        int interfaces_unowned = candidate_interfaces_are_unowned();
        if (interfaces_unowned < 0) {
            return 70;
        }
        if (!interfaces_unowned) {
            fputs("descriptor-reader: candidate interfaces are in use. Choose "
                "Quit Completely in DockBridge, verify it has stopped, then retry.\n",
                stderr);
            return 73;
        }

        uint32_t revision = 0;
        uint32_t speed = 0;
        io_service_t service = copy_single_target_device(&revision, &speed);
        if (service == IO_OBJECT_NULL) {
            fputs("descriptor-reader: exactly one allowlisted 17e9:4323 device "
                "with revision and speed metadata is required\n", stderr);
            return 69;
        }
        if (entry_has_owner(service)) {
            IOObjectRelease(service);
            fputs("descriptor-reader: the allowlisted USB device is in use. "
                "No ownership request was made.\n", stderr);
            return 73;
        }

        puts("DockBridge standard USB descriptor reader (opt-in)");
        puts("The exact device is opened temporarily without capture or seize; "
            "a standard GET_DESCRIPTOR read may occur.");
        puts("No interface is claimed; no configuration, reset, vendor request, "
            "firmware operation, or endpoint transfer is performed.");

        NSError *error = nil;
        IOUSBHostDevice *device = [[IOUSBHostDevice alloc]
            initWithIOService:service
                       options:IOUSBHostObjectInitOptionsNone
                         queue:nil
                         error:&error
               interestHandler:nil];
        IOObjectRelease(service);
        if (device == nil) {
            fprintf(stderr, "descriptor-reader: standard descriptor access "
                "could not be opened (%ld)\n", (long)error.code);
            return 77;
        }

        const IOUSBConfigurationDescriptor *configuration =
            device.configurationDescriptor;
        if (configuration == NULL) {
            configuration = [device configurationDescriptorWithIndex:0
                error:&error];
        }
        int success = 0;
        if (configuration == NULL) {
            fprintf(stderr, "descriptor-reader: standard configuration "
                "descriptor read failed (%ld)\n", (long)error.code);
        } else {
            printf("device 17e9:4323 bcdDevice=0x%04x speed-code=%u "
                "configuration=%u\n", revision, speed,
                configuration->bConfigurationValue);
            success = print_candidate_descriptors(configuration, speed);
        }
        [device destroy];
        return success ? 0 : 65;
    }
}
