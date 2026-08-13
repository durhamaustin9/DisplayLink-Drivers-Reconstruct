#include "descriptor_model.h"

#include <IOKit/usb/USB.h>
#include <stddef.h>

int
db_endpoint_facts(uint32_t speed, const IOUSBEndpointDescriptor *endpoint,
    const IOUSBSuperSpeedEndpointCompanionDescriptor *companion,
    DBEndpointFacts *facts)
{
    if (endpoint == NULL || facts == NULL ||
        speed > kUSBDeviceSpeedSuperPlusBy2 ||
        endpoint->bLength < sizeof(IOUSBEndpointDescriptor) ||
        endpoint->bDescriptorType != kIOUSBDescriptorTypeEndpoint ||
        (endpoint->bEndpointAddress & 0x70U) != 0 ||
        (endpoint->bEndpointAddress & 0x0fU) == 0) {
        return 0;
    }
    if (speed >= kUSBDeviceSpeedSuper &&
        (companion == NULL ||
            companion->bLength < sizeof(IOUSBSuperSpeedEndpointCompanionDescriptor) ||
            companion->bDescriptorType !=
                kIOUSBDescriptorTypeSuperSpeedUSBEndpointCompanion ||
            companion->bMaxBurst > 15)) {
        return 0;
    }

    uint8_t transfer_type = IOUSBGetEndpointType(endpoint);
    if (transfer_type > kIOUSBEndpointDescriptorTransferTypeInterrupt) {
        return 0;
    }

    uint16_t maximum_packet_size =
        IOUSBGetEndpointMaxPacketSize(speed, endpoint);
    if (maximum_packet_size == 0) {
        return 0;
    }

    *facts = (DBEndpointFacts) {
        .address = IOUSBGetEndpointAddress(endpoint),
        .number = IOUSBGetEndpointNumber(endpoint),
        .direction = IOUSBGetEndpointDirection(endpoint),
        .transfer_type = transfer_type,
        .maximum_packet_size = maximum_packet_size,
        .raw_interval = endpoint->bInterval,
        .interval_microframes =
            IOUSBGetEndpointIntervalMicroframes(speed, endpoint),
        .interval_frames = IOUSBGetEndpointIntervalFrames(speed, endpoint),
        .burst_size_bytes = IOUSBGetEndpointBurstSize(
            speed, endpoint, companion, NULL),
        .maximum_streams = companion == NULL ? 0 :
            IOUSBGetEndpointMaxStreams(speed, endpoint, companion),
        .maximum_burst_packets = companion == NULL ? 1 :
            (uint16_t)companion->bMaxBurst + 1U,
        .has_superspeed_companion = companion != NULL
    };
    return 1;
}

const char *
db_endpoint_direction_name(uint8_t direction)
{
    if (direction == kIOUSBEndpointDirectionIn) {
        return "in";
    }
    if (direction == kIOUSBEndpointDirectionOut) {
        return "out";
    }
    return "invalid";
}

const char *
db_endpoint_transfer_type_name(uint8_t transfer_type)
{
    switch (transfer_type) {
    case kIOUSBEndpointDescriptorTransferTypeControl:
        return "control";
    case kIOUSBEndpointDescriptorTransferTypeIsochronous:
        return "isochronous";
    case kIOUSBEndpointDescriptorTransferTypeBulk:
        return "bulk";
    case kIOUSBEndpointDescriptorTransferTypeInterrupt:
        return "interrupt";
    default:
        return "invalid";
    }
}
