#include "state_machine.h"

#include <stdatomic.h>
#include <string.h>

static _Atomic uint64_t next_machine_generation = UINT64_C(1);

static uint64_t
take_machine_generation(void)
{
    uint64_t current = atomic_load_explicit(&next_machine_generation,
        memory_order_relaxed);
    for (;;) {
        if (current == UINT64_MAX) {
            return 0U;
        }
        uint64_t next = current + UINT64_C(1);
        if (atomic_compare_exchange_weak_explicit(&next_machine_generation,
                &current, next, memory_order_relaxed,
                memory_order_relaxed)) {
            return current;
        }
    }
}

static int
identity_is_exact(const DBMachineDeviceIdentity *identity)
{
    return identity->vendor_id == DB_MACHINE_VENDOR_ID &&
        identity->product_id == DB_MACHINE_PRODUCT_ID &&
        identity->revision == DB_MACHINE_DEVICE_REVISION;
}

static int
topology_is_exact(const DBMachineTopology *topology)
{
    return topology->display_interface == DB_MACHINE_DISPLAY_INTERFACE &&
        topology->display_class == 0xff &&
        topology->display_subclass == 0 &&
        topology->display_protocol == 3 &&
        topology->auxiliary_interface == DB_MACHINE_AUXILIARY_INTERFACE &&
        topology->auxiliary_endpoint_count == 0 &&
        topology->endpoint_out == DB_MACHINE_ENDPOINT_OUT &&
        topology->endpoint_out_type == DB_MACHINE_TRANSFER_TYPE_BULK &&
        topology->endpoint_out_max_packet == DB_MACHINE_MAX_PACKET_SIZE &&
        topology->endpoint_in == DB_MACHINE_ENDPOINT_IN &&
        topology->endpoint_in_type == DB_MACHINE_TRANSFER_TYPE_BULK &&
        topology->endpoint_in_max_packet == DB_MACHINE_MAX_PACKET_SIZE &&
        topology->endpoint_out_burst_packets == 1 &&
        topology->endpoint_in_burst_packets == 1 &&
        topology->endpoint_out_streams == 0 &&
        topology->endpoint_in_streams == 0;
}

void
db_machine_initialize(DBMachine *machine, DBTransport *transport)
{
    if (machine == NULL) {
        return;
    }
    *machine = (DBMachine) {
        .state = DB_MACHINE_OFFLINE,
        .transport = transport,
        .last_transport_result = DB_TRANSPORT_OK
    };
}

int
db_machine_is_exact_verified(const DBMachine *machine)
{
    return machine != NULL && machine->transport != NULL &&
        machine->transport->kind == DB_TRANSPORT_KIND_FAKE &&
        db_transport_is_open(machine->transport) &&
        machine->generation != 0U &&
        machine->transport_lifecycle_epoch != 0U &&
        db_transport_lifecycle_epoch(machine->transport) ==
            machine->transport_lifecycle_epoch &&
        machine->state == DB_MACHINE_TOPOLOGY_VERIFIED &&
        identity_is_exact(&machine->identity) &&
        topology_is_exact(&machine->topology);
}

DBMachineResult
db_machine_attach(DBMachine *machine, const DBMachineDeviceIdentity *identity)
{
    if (machine == NULL || identity == NULL || machine->transport == NULL) {
        return DB_MACHINE_INVALID_ARGUMENT;
    }
    if (machine->state != DB_MACHINE_OFFLINE) {
        return DB_MACHINE_WRONG_STATE;
    }
    if (!identity_is_exact(identity)) {
        return DB_MACHINE_UNSUPPORTED_DEVICE;
    }
    if (machine->transport->kind != DB_TRANSPORT_KIND_FAKE) {
        return DB_MACHINE_REAL_TRANSPORT_DISABLED;
    }

    uint64_t generation = take_machine_generation();
    if (generation == 0U) {
        machine->state = DB_MACHINE_FAULT;
        return DB_MACHINE_TRANSPORT_ERROR;
    }
    DBTransportResult opened = db_transport_open(machine->transport);
    machine->last_transport_result = opened;
    if (opened != DB_TRANSPORT_OK) {
        machine->state = DB_MACHINE_FAULT;
        return DB_MACHINE_TRANSPORT_ERROR;
    }

    machine->identity = *identity;
    machine->state = DB_MACHINE_ATTACHED;
    machine->generation = generation;
    machine->transport_lifecycle_epoch =
        db_transport_lifecycle_epoch(machine->transport);
    if (machine->transport_lifecycle_epoch == 0U) {
        db_transport_close(machine->transport);
        machine->last_transport_result = DB_TRANSPORT_INVALID_ARGUMENT;
        machine->state = DB_MACHINE_FAULT;
        return DB_MACHINE_TRANSPORT_ERROR;
    }
    return DB_MACHINE_OK;
}

DBMachineResult
db_machine_verify_topology(DBMachine *machine,
    const DBMachineTopology *topology)
{
    if (machine == NULL || topology == NULL) {
        return DB_MACHINE_INVALID_ARGUMENT;
    }
    if (machine->state != DB_MACHINE_ATTACHED) {
        return DB_MACHINE_WRONG_STATE;
    }
    if (!topology_is_exact(topology)) {
        return DB_MACHINE_UNSUPPORTED_TOPOLOGY;
    }
    machine->topology = *topology;
    machine->state = DB_MACHINE_TOPOLOGY_VERIFIED;
    return DB_MACHINE_OK;
}

DBMachineResult
db_machine_request_activation(DBMachine *machine)
{
    if (machine == NULL) {
        return DB_MACHINE_INVALID_ARGUMENT;
    }
    if (machine->state != DB_MACHINE_TOPOLOGY_VERIFIED &&
        machine->state != DB_MACHINE_BLOCKED_PROTOCOL_UNDOCUMENTED) {
        return DB_MACHINE_WRONG_STATE;
    }

    /* This is the intentional write barrier. No protocol message is known or
       constructed, and the transport write callback is never reached. */
    machine->state = DB_MACHINE_BLOCKED_PROTOCOL_UNDOCUMENTED;
    return DB_MACHINE_PROTOCOL_UNDOCUMENTED;
}

DBMachineResult
db_machine_report_transport_fault(DBMachine *machine, DBTransportResult result)
{
    if (machine == NULL || result == DB_TRANSPORT_OK) {
        return DB_MACHINE_INVALID_ARGUMENT;
    }
    machine->last_transport_result = result;
    machine->state = DB_MACHINE_FAULT;
    return DB_MACHINE_TRANSPORT_ERROR;
}

DBMachineResult
db_machine_detach(DBMachine *machine)
{
    if (machine == NULL || machine->transport == NULL) {
        return DB_MACHINE_INVALID_ARGUMENT;
    }
    db_transport_close(machine->transport);
    memset(&machine->identity, 0, sizeof(machine->identity));
    memset(&machine->topology, 0, sizeof(machine->topology));
    machine->transport_lifecycle_epoch = 0U;
    machine->state = DB_MACHINE_OFFLINE;
    machine->last_transport_result = DB_TRANSPORT_OK;
    return DB_MACHINE_OK;
}

const char *
db_machine_state_name(DBMachineState state)
{
    switch (state) {
    case DB_MACHINE_OFFLINE:
        return "offline";
    case DB_MACHINE_ATTACHED:
        return "attached";
    case DB_MACHINE_TOPOLOGY_VERIFIED:
        return "topology-verified";
    case DB_MACHINE_BLOCKED_PROTOCOL_UNDOCUMENTED:
        return "blocked-protocol-undocumented";
    case DB_MACHINE_FAULT:
        return "fault";
    }
    return "invalid-state";
}

const char *
db_machine_result_name(DBMachineResult result)
{
    switch (result) {
    case DB_MACHINE_OK:
        return "ok";
    case DB_MACHINE_INVALID_ARGUMENT:
        return "invalid-argument";
    case DB_MACHINE_WRONG_STATE:
        return "wrong-state";
    case DB_MACHINE_UNSUPPORTED_DEVICE:
        return "unsupported-device";
    case DB_MACHINE_UNSUPPORTED_TOPOLOGY:
        return "unsupported-topology";
    case DB_MACHINE_REAL_TRANSPORT_DISABLED:
        return "real-transport-disabled";
    case DB_MACHINE_TRANSPORT_ERROR:
        return "transport-error";
    case DB_MACHINE_PROTOCOL_UNDOCUMENTED:
        return "protocol-undocumented";
    }
    return "invalid-result";
}
