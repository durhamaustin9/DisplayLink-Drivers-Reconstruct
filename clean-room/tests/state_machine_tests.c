#include "fake_transport.h"
#include "state_machine.h"

#include <assert.h>
#include <string.h>

static DBMachineDeviceIdentity
identity(void)
{
    return (DBMachineDeviceIdentity) {
        DB_MACHINE_VENDOR_ID,
        DB_MACHINE_PRODUCT_ID,
        DB_MACHINE_DEVICE_REVISION
    };
}

static DBMachineTopology
topology(void)
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

static void
assert_topology_rejected(DBMachineTopology candidate)
{
    DBFakeTransport fake = {0};
    DBTransport transport = {0};
    DBMachine machine = {0};
    DBMachineDeviceIdentity exact_identity = identity();
    db_fake_transport_initialize(&fake, &transport);
    db_machine_initialize(&machine, &transport);
    assert(db_machine_attach(&machine, &exact_identity) == DB_MACHINE_OK);
    assert(db_machine_verify_topology(&machine, &candidate) ==
        DB_MACHINE_UNSUPPORTED_TOPOLOGY);
    assert(machine.state == DB_MACHINE_ATTACHED);
    assert(fake.write_attempt_count == 0);
    assert(db_machine_detach(&machine) == DB_MACHINE_OK);
}

int
main(void)
{
    DBFakeTransport fake = {0};
    DBTransport transport = {0};
    DBMachine machine = {0};
    DBMachineDeviceIdentity exact_identity = identity();
    DBMachineTopology exact_topology = topology();

    db_fake_transport_initialize(&fake, &transport);
    db_machine_initialize(&machine, &transport);
    assert(machine.state == DB_MACHINE_OFFLINE);
    assert(db_machine_request_activation(&machine) == DB_MACHINE_WRONG_STATE);

    DBMachineDeviceIdentity wrong_identity = exact_identity;
    wrong_identity.product_id = 0x6006;
    assert(db_machine_attach(&machine, &wrong_identity) ==
        DB_MACHINE_UNSUPPORTED_DEVICE);
    wrong_identity = exact_identity;
    wrong_identity.revision = 0x3157;
    assert(db_machine_attach(&machine, &wrong_identity) ==
        DB_MACHINE_UNSUPPORTED_DEVICE);

    DBTransport nonfake = transport;
    nonfake.kind = (DBTransportKind)99;
    machine.transport = &nonfake;
    assert(db_machine_attach(&machine, &exact_identity) ==
        DB_MACHINE_REAL_TRANSPORT_DISABLED);
    machine.transport = &transport;

    assert(db_machine_attach(&machine, &exact_identity) == DB_MACHINE_OK);
    assert(machine.state == DB_MACHINE_ATTACHED);
    assert(machine.generation == 1);
    assert(fake.open_count == 1);
    assert(db_machine_attach(&machine, &exact_identity) == DB_MACHINE_WRONG_STATE);
    assert(db_machine_verify_topology(&machine, &exact_topology) == DB_MACHINE_OK);
    assert(machine.state == DB_MACHINE_TOPOLOGY_VERIFIED);
    assert(db_machine_request_activation(&machine) ==
        DB_MACHINE_PROTOCOL_UNDOCUMENTED);
    assert(machine.state == DB_MACHINE_BLOCKED_PROTOCOL_UNDOCUMENTED);
    assert(db_machine_request_activation(&machine) ==
        DB_MACHINE_PROTOCOL_UNDOCUMENTED);
    assert(fake.write_attempt_count == 0);
    assert(fake.write_success_count == 0);
    assert(fake.read_attempt_count == 0);
    assert(db_machine_detach(&machine) == DB_MACHINE_OK);
    assert(machine.state == DB_MACHINE_OFFLINE);
    assert(fake.close_count == 1);

    DBMachineTopology wrong_topology = exact_topology;
    wrong_topology.endpoint_out = 0x03;
    assert_topology_rejected(wrong_topology);
    wrong_topology = exact_topology;
    wrong_topology.endpoint_in_max_packet = 512;
    assert_topology_rejected(wrong_topology);
    wrong_topology = exact_topology;
    wrong_topology.endpoint_out_burst_packets = 2;
    assert_topology_rejected(wrong_topology);
    wrong_topology = exact_topology;
    wrong_topology.endpoint_in_streams = 1;
    assert_topology_rejected(wrong_topology);
    wrong_topology = exact_topology;
    wrong_topology.display_class = 0xfe;
    assert_topology_rejected(wrong_topology);

    db_fake_transport_disconnect(&fake);
    assert(db_machine_attach(&machine, &exact_identity) ==
        DB_MACHINE_TRANSPORT_ERROR);
    assert(machine.state == DB_MACHINE_FAULT);
    assert(machine.last_transport_result == DB_TRANSPORT_DISCONNECTED);
    assert(db_machine_detach(&machine) == DB_MACHINE_OK);
    db_fake_transport_reconnect(&fake);
    assert(db_machine_attach(&machine, &exact_identity) == DB_MACHINE_OK);
    assert(machine.generation == 2);
    assert(db_machine_report_transport_fault(&machine,
        DB_TRANSPORT_DISCONNECTED) == DB_MACHINE_TRANSPORT_ERROR);
    assert(machine.state == DB_MACHINE_FAULT);
    assert(db_machine_report_transport_fault(&machine, DB_TRANSPORT_OK) ==
        DB_MACHINE_INVALID_ARGUMENT);
    assert(db_machine_detach(&machine) == DB_MACHINE_OK);

    assert(db_machine_attach(NULL, &exact_identity) == DB_MACHINE_INVALID_ARGUMENT);
    assert(db_machine_verify_topology(NULL, &exact_topology) ==
        DB_MACHINE_INVALID_ARGUMENT);
    assert(strcmp(db_machine_state_name(DB_MACHINE_TOPOLOGY_VERIFIED),
        "topology-verified") == 0);
    assert(strcmp(db_machine_result_name(DB_MACHINE_PROTOCOL_UNDOCUMENTED),
        "protocol-undocumented") == 0);
    return 0;
}
