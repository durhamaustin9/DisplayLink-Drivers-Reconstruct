#include "fake_transport.h"
#include "state_machine.h"

#include <stdio.h>

static DBMachineDeviceIdentity
observed_identity(void)
{
    return (DBMachineDeviceIdentity) {
        .vendor_id = DB_MACHINE_VENDOR_ID,
        .product_id = DB_MACHINE_PRODUCT_ID,
        .revision = DB_MACHINE_DEVICE_REVISION
    };
}

static DBMachineTopology
observed_topology(void)
{
    return (DBMachineTopology) {
        .display_interface = DB_MACHINE_DISPLAY_INTERFACE,
        .display_class = 0xff,
        .display_subclass = 0,
        .display_protocol = 3,
        .auxiliary_interface = DB_MACHINE_AUXILIARY_INTERFACE,
        .auxiliary_endpoint_count = 0,
        .endpoint_out = DB_MACHINE_ENDPOINT_OUT,
        .endpoint_out_type = DB_MACHINE_TRANSFER_TYPE_BULK,
        .endpoint_out_max_packet = DB_MACHINE_MAX_PACKET_SIZE,
        .endpoint_in = DB_MACHINE_ENDPOINT_IN,
        .endpoint_in_type = DB_MACHINE_TRANSFER_TYPE_BULK,
        .endpoint_in_max_packet = DB_MACHINE_MAX_PACKET_SIZE,
        .endpoint_out_burst_packets = 1,
        .endpoint_in_burst_packets = 1,
        .endpoint_out_streams = 0,
        .endpoint_in_streams = 0
    };
}

static int
transition(const char *name, DBMachineResult result, const DBMachine *machine)
{
    printf("%-18s result=%-22s state=%s\n", name,
        db_machine_result_name(result), db_machine_state_name(machine->state));
    return result == DB_MACHINE_OK;
}

int
main(void)
{
    DBFakeTransport fake = {0};
    DBTransport transport = {0};
    DBMachine machine = {0};
    DBMachineDeviceIdentity identity = observed_identity();
    DBMachineTopology topology = observed_topology();

    db_fake_transport_initialize(&fake, &transport);
    db_machine_initialize(&machine, &transport);
    puts("DockBridge fake USB transport lab");
    puts("In-memory only; no device access and no hardware writes.");

    if (!transition("attach", db_machine_attach(&machine, &identity), &machine)) {
        return 1;
    }
    if (!transition("verify-topology",
        db_machine_verify_topology(&machine, &topology), &machine)) {
        return 1;
    }

    DBMachineResult activation = db_machine_request_activation(&machine);
    printf("%-18s result=%-22s state=%s\n", "request-activation",
        db_machine_result_name(activation), db_machine_state_name(machine.state));
    if (activation != DB_MACHINE_PROTOCOL_UNDOCUMENTED ||
        fake.write_attempt_count != 0 || fake.write_success_count != 0) {
        fputs("fake-lab: protocol gate or zero-write invariant failed\n", stderr);
        return 1;
    }

    if (!transition("detach", db_machine_detach(&machine), &machine)) {
        return 1;
    }
    printf("transport opens=%zu closes=%zu write-attempts=%zu successful-writes=%zu\n",
        fake.open_count, fake.close_count, fake.write_attempt_count,
        fake.write_success_count);
    puts("PASS: activation is blocked and the state machine performed zero writes.");
    return 0;
}
