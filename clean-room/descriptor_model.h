#ifndef DOCKBRIDGE_DESCRIPTOR_MODEL_H
#define DOCKBRIDGE_DESCRIPTOR_MODEL_H

#include <IOUSBHost/AppleUSBDescriptorParsing.h>
#include <stdint.h>

typedef struct {
    uint8_t address;
    uint8_t number;
    uint8_t direction;
    uint8_t transfer_type;
    uint16_t maximum_packet_size;
    uint8_t raw_interval;
    uint32_t interval_microframes;
    uint32_t interval_frames;
    uint32_t burst_size_bytes;
    uint32_t maximum_streams;
    uint16_t maximum_burst_packets;
    int has_superspeed_companion;
} DBEndpointFacts;

int db_endpoint_facts(uint32_t speed,
    const IOUSBEndpointDescriptor *endpoint,
    const IOUSBSuperSpeedEndpointCompanionDescriptor *companion,
    DBEndpointFacts *facts);
const char *db_endpoint_direction_name(uint8_t direction);
const char *db_endpoint_transfer_type_name(uint8_t transfer_type);

#endif
