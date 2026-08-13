#include "descriptor_model.h"

#include <IOKit/usb/AppleUSBDefinitions.h>
#include <assert.h>
#include <string.h>

int
main(void)
{
    IOUSBEndpointDescriptor out = {
        .bLength = sizeof(IOUSBEndpointDescriptor),
        .bDescriptorType = kIOUSBDescriptorTypeEndpoint,
        .bEndpointAddress = 0x02,
        .bmAttributes = kIOUSBEndpointDescriptorTransferTypeBulk,
        .wMaxPacketSize = 1024,
        .bInterval = 0
    };
    IOUSBSuperSpeedEndpointCompanionDescriptor companion = {
        .bLength = sizeof(IOUSBSuperSpeedEndpointCompanionDescriptor),
        .bDescriptorType =
            kIOUSBDescriptorTypeSuperSpeedUSBEndpointCompanion,
        .bMaxBurst = 15,
        .bmAttributes = 0,
        .wBytesPerInterval = 0
    };
    DBEndpointFacts facts = {0};
    assert(db_endpoint_facts(4, &out, &companion, &facts));
    assert(facts.address == 0x02);
    assert(facts.number == 2);
    assert(facts.direction == kIOUSBEndpointDirectionOut);
    assert(facts.transfer_type == kIOUSBEndpointDescriptorTransferTypeBulk);
    assert(facts.maximum_packet_size == 1024);
    assert(facts.maximum_burst_packets == 16);
    assert(facts.has_superspeed_companion);
    assert(strcmp(db_endpoint_direction_name(facts.direction), "out") == 0);
    assert(strcmp(db_endpoint_transfer_type_name(facts.transfer_type), "bulk") == 0);

    IOUSBEndpointDescriptor in = out;
    in.bEndpointAddress = 0x84;
    assert(db_endpoint_facts(4, &in, &companion, &facts));
    assert(facts.address == 0x84);
    assert(facts.number == 4);
    assert(facts.direction == kIOUSBEndpointDirectionIn);
    assert(facts.maximum_burst_packets == 16);
    assert(facts.has_superspeed_companion);
    assert(strcmp(db_endpoint_direction_name(facts.direction), "in") == 0);

    assert(!db_endpoint_facts(4, &in, NULL, &facts));
    assert(db_endpoint_facts(2, &in, NULL, &facts));
    assert(facts.maximum_burst_packets == 1);
    assert(!facts.has_superspeed_companion);

    IOUSBEndpointDescriptor invalid = out;
    invalid.bDescriptorType = kIOUSBDescriptorTypeInterface;
    assert(!db_endpoint_facts(4, &invalid, NULL, &facts));
    invalid = out;
    invalid.bEndpointAddress = 0x72;
    assert(!db_endpoint_facts(4, &invalid, &companion, &facts));
    invalid = out;
    invalid.wMaxPacketSize = 0;
    assert(!db_endpoint_facts(4, &invalid, &companion, &facts));
    IOUSBSuperSpeedEndpointCompanionDescriptor invalid_companion = companion;
    invalid_companion.bMaxBurst = 16;
    assert(!db_endpoint_facts(4, &out, &invalid_companion, &facts));
    assert(!db_endpoint_facts(6, &out, &companion, &facts));
    assert(!db_endpoint_facts(4, NULL, NULL, &facts));
    assert(!db_endpoint_facts(4, &out, NULL, NULL));
    assert(strcmp(db_endpoint_direction_name(2), "invalid") == 0);
    assert(strcmp(db_endpoint_transfer_type_name(9), "invalid") == 0);
    return 0;
}
