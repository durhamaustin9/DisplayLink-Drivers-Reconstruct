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

    if (property != 0 && CFGetTypeID(property) == CFNumberGetTypeID() &&
        CFNumberGetValue((CFNumberRef)property, kCFNumberSInt64Type, &number) &&
        number >= 0 && number <= UINT32_MAX) {
        *value = (uint32_t)number;
        found = 1;
    }
    if (property != 0) {
        CFRelease(property);
    }
    return found;
}

static int
device_identity(io_registry_entry_t entry, uint32_t *vendor_id,
    uint32_t *product_id)
{
    return read_u32(entry, CFSTR("idVendor"), vendor_id) &&
        read_u32(entry, CFSTR("idProduct"), product_id);
}

static int
is_target_device(io_registry_entry_t entry)
{
    uint32_t vendor_id = 0;
    uint32_t product_id = 0;

    return device_identity(entry, &vendor_id, &product_id) &&
        db_probe_matches_device(vendor_id, product_id);
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
        if (is_target_device(current)) {
            result = 1;
            break;
        }
    }
    if (owns_current) {
        IOObjectRelease(current);
    }
    return result;
}

static uint32_t
optional_u32(io_registry_entry_t entry, CFStringRef key)
{
    uint32_t value = 0;
    (void)read_u32(entry, key, &value);
    return value;
}

static int
print_target_devices(void)
{
    io_iterator_t iterator = IO_OBJECT_NULL;
    io_registry_entry_t entry = IO_OBJECT_NULL;
    CFMutableDictionaryRef match = IOServiceMatching("IOUSBHostDevice");
    int found = 0;

    if (match == 0 || IOServiceGetMatchingServices(kIOMainPortDefault, match,
        &iterator) != KERN_SUCCESS) {
        fputs("probe: could not enumerate USB devices\n", stderr);
        return -1;
    }

    while ((entry = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        if (is_target_device(entry)) {
            uint32_t revision = optional_u32(entry, CFSTR("bcdDevice"));
            uint32_t speed = optional_u32(entry, CFSTR("USBSpeed"));
            printf("device 17e9:4323 found (bcdDevice=0x%04x, USB speed code=%u)\n",
                revision, speed);
            found = 1;
        }
        IOObjectRelease(entry);
    }
    IOObjectRelease(iterator);
    return found;
}

static int
print_target_interfaces(void)
{
    io_iterator_t iterator = IO_OBJECT_NULL;
    io_registry_entry_t entry = IO_OBJECT_NULL;
    CFMutableDictionaryRef match = IOServiceMatching("IOUSBHostInterface");
    int count = 0;

    if (match == 0 || IOServiceGetMatchingServices(kIOMainPortDefault, match,
        &iterator) != KERN_SUCCESS) {
        fputs("probe: could not enumerate USB interfaces\n", stderr);
        return -1;
    }

    while ((entry = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        if (interface_belongs_to_target(entry)) {
            DBProbeInterface descriptor = {
                .number = optional_u32(entry, CFSTR("bInterfaceNumber")),
                .interface_class = optional_u32(entry, CFSTR("bInterfaceClass")),
                .subclass = optional_u32(entry, CFSTR("bInterfaceSubClass")),
                .protocol = optional_u32(entry, CFSTR("bInterfaceProtocol")),
                .alternate_setting = optional_u32(entry, CFSTR("bAlternateSetting")),
                .endpoint_count = optional_u32(entry, CFSTR("bNumEndpoints"))
            };
            printf("interface %u: class=0x%02x subclass=0x%02x protocol=0x%02x "
                "alt=%u endpoints=%u — %s\n",
                descriptor.number, descriptor.interface_class,
                descriptor.subclass, descriptor.protocol,
                descriptor.alternate_setting, descriptor.endpoint_count,
                db_probe_interface_role(&descriptor));
            count++;
        }
        IOObjectRelease(entry);
    }
    IOObjectRelease(iterator);
    return count;
}

static void
usage(const char *program)
{
    fprintf(stderr, "usage: %s\n", program);
}

int
main(int argc, char **argv)
{
    int devices;
    int interfaces;

    if (argc != 1 || (argc > 1 && strcmp(argv[1], "--help") == 0)) {
        usage(argv[0]);
        return argc > 1 && strcmp(argv[1], "--help") == 0 ? 0 : 64;
    }

    puts("DockBridge clean-room descriptor probe (read-only)");
    puts("No interface is opened, claimed, or sent a transfer.");
    devices = print_target_devices();
    if (devices < 0) {
        return 70;
    }
    if (devices == 0) {
        fputs("probe: allowlisted USB-display dock 17e9:4323 was not found\n",
            stderr);
        return 2;
    }
    interfaces = print_target_interfaces();
    if (interfaces < 0) {
        return 70;
    }
    printf("observed %d interface%s; descriptor inspection complete\n",
        interfaces, interfaces == 1 ? "" : "s");
    return 0;
}
