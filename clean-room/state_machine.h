#ifndef DOCKBRIDGE_STATE_MACHINE_H
#define DOCKBRIDGE_STATE_MACHINE_H

#include "transport.h"

#include <stdint.h>

enum {
    DB_MACHINE_VENDOR_ID = 0x17e9,
    DB_MACHINE_PRODUCT_ID = 0x4323,
    DB_MACHINE_DEVICE_REVISION = 0x3156,
    DB_MACHINE_DISPLAY_INTERFACE = 0,
    DB_MACHINE_AUXILIARY_INTERFACE = 1,
    DB_MACHINE_ENDPOINT_OUT = 0x02,
    DB_MACHINE_ENDPOINT_IN = 0x84,
    DB_MACHINE_MAX_PACKET_SIZE = 1024,
    DB_MACHINE_TRANSFER_TYPE_BULK = 2
};

typedef enum {
    DB_MACHINE_OFFLINE = 0,
    DB_MACHINE_ATTACHED,
    DB_MACHINE_TOPOLOGY_VERIFIED,
    DB_MACHINE_BLOCKED_PROTOCOL_UNDOCUMENTED,
    DB_MACHINE_FAULT
} DBMachineState;

typedef enum {
    DB_MACHINE_OK = 0,
    DB_MACHINE_INVALID_ARGUMENT,
    DB_MACHINE_WRONG_STATE,
    DB_MACHINE_UNSUPPORTED_DEVICE,
    DB_MACHINE_UNSUPPORTED_TOPOLOGY,
    DB_MACHINE_REAL_TRANSPORT_DISABLED,
    DB_MACHINE_TRANSPORT_ERROR,
    DB_MACHINE_PROTOCOL_UNDOCUMENTED
} DBMachineResult;

typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t revision;
} DBMachineDeviceIdentity;

typedef struct {
    uint8_t display_interface;
    uint8_t display_class;
    uint8_t display_subclass;
    uint8_t display_protocol;
    uint8_t auxiliary_interface;
    uint8_t auxiliary_endpoint_count;
    uint8_t endpoint_out;
    uint8_t endpoint_out_type;
    uint16_t endpoint_out_max_packet;
    uint8_t endpoint_in;
    uint8_t endpoint_in_type;
    uint16_t endpoint_in_max_packet;
    uint16_t endpoint_out_burst_packets;
    uint16_t endpoint_in_burst_packets;
    uint32_t endpoint_out_streams;
    uint32_t endpoint_in_streams;
} DBMachineTopology;

typedef struct {
    DBMachineState state;
    DBTransport *transport;
    DBMachineDeviceIdentity identity;
    DBMachineTopology topology;
    uint64_t generation;
    uint64_t transport_lifecycle_epoch;
    DBTransportResult last_transport_result;
} DBMachine;

void db_machine_initialize(DBMachine *machine, DBTransport *transport);
int db_machine_is_exact_verified(const DBMachine *machine);
DBMachineResult db_machine_attach(DBMachine *machine,
    const DBMachineDeviceIdentity *identity);
DBMachineResult db_machine_verify_topology(DBMachine *machine,
    const DBMachineTopology *topology);
DBMachineResult db_machine_request_activation(DBMachine *machine);
DBMachineResult db_machine_report_transport_fault(DBMachine *machine,
    DBTransportResult result);
DBMachineResult db_machine_detach(DBMachine *machine);
const char *db_machine_state_name(DBMachineState state);
const char *db_machine_result_name(DBMachineResult result);

#endif
