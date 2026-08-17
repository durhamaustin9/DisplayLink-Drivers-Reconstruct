#include "fake_transport.h"
#include "qualified_sequences.h"
#include "traffic_regime.h"
#include "transition_parser.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    DBFakeTransport fake;
    DBTransport transport;
    DBMachine machine;
} LabFixture;

static int
initialize_fixture(LabFixture *fixture)
{
    DBMachineDeviceIdentity identity = {
        DB_MACHINE_VENDOR_ID,
        DB_MACHINE_PRODUCT_ID,
        DB_MACHINE_DEVICE_REVISION
    };
    DBMachineTopology topology = {
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
    memset(fixture, 0, sizeof(*fixture));
    db_fake_transport_initialize(&fixture->fake, &fixture->transport);
    db_machine_initialize(&fixture->machine, &fixture->transport);
    return db_machine_attach(&fixture->machine, &identity) == DB_MACHINE_OK &&
        db_machine_verify_topology(&fixture->machine, &topology) ==
            DB_MACHINE_OK &&
        db_machine_is_exact_verified(&fixture->machine);
}

static void
fill_synthetic(uint8_t *bytes, size_t length, uint8_t pattern,
    DBProtocolDirection direction)
{
    memset(bytes, pattern, length);
    if (direction == DB_PROTOCOL_DIRECTION_IN) {
        size_t body_length = length - DB_PROTOCOL_IN_PREFIX_SIZE;
        bytes[0] = 0U;
        bytes[1] = 0U;
        bytes[2] = (uint8_t)(body_length & 0xffU);
        bytes[3] = (uint8_t)((body_length >> 8U) & 0xffU);
    }
}

static int
transfer_one(LabFixture *fixture, DBTransitionParser *parser,
    const DBPartialOrderRole *role, uint8_t pattern)
{
    uint8_t authored[DB_FAKE_MAX_CHUNK_SIZE];
    uint8_t received[DB_FAKE_MAX_CHUNK_SIZE];
    size_t length = role->allowed_lengths[0];
    size_t received_length = 0U;
    if (length > sizeof(authored)) {
        return 0;
    }
    fill_synthetic(authored, length, pattern, role->direction);

    DBTransportResult result;
    if (role->direction == DB_PROTOCOL_DIRECTION_OUT) {
        result = db_transport_write(&fixture->transport,
            DB_FAKE_ENDPOINT_OUT, authored, length);
        if (result != DB_TRANSPORT_OK) {
            return 0;
        }
        result = db_fake_transport_take_outbound(&fixture->fake, received,
            sizeof(received), &received_length);
    } else {
        result = db_fake_transport_inject_inbound(&fixture->fake, authored,
            length);
        if (result != DB_TRANSPORT_OK) {
            return 0;
        }
        result = db_transport_read(&fixture->transport, DB_FAKE_ENDPOINT_IN,
            received, sizeof(received), &received_length);
    }
    if (result != DB_TRANSPORT_OK || received_length != length ||
        memcmp(authored, received, length) != 0) {
        return 0;
    }

    DBProtocolTransferMetadata metadata = {
        .direction = role->direction,
        .endpoint = role->endpoint,
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,
        .succeeded = 1U,
        .length = received_length
    };
    if (role->direction == DB_PROTOCOL_DIRECTION_IN) {
        memcpy(metadata.in_prefix, received, DB_PROTOCOL_IN_PREFIX_SIZE);
    }
    DBTransitionResult accepted = db_transition_parser_accept(parser,
        &metadata);
    return accepted == DB_TRANSITION_RESULT_OK ||
        accepted == DB_TRANSITION_RESULT_COMPLETE;
}

static int
run_transition(LabFixture *fixture, DBTransitionKind parser_kind,
    DBQualifiedSequenceKind sequence_kind, uint8_t pattern)
{
    DBQualifiedSequence sequence = {0};
    DBTransitionParser parser = {0};
    if (!db_qualified_sequence_get(sequence_kind, &sequence) ||
        db_transition_parser_initialize(&parser, parser_kind,
            &fixture->machine) != DB_TRANSITION_RESULT_OK) {
        return 0;
    }
    for (size_t index = 0; index < sequence.role_count; ++index) {
        if (!transfer_one(fixture, &parser, &sequence.roles[index],
                pattern)) {
            return 0;
        }
    }
    if (db_transition_parser_finish(&parser) !=
            DB_TRANSITION_RESULT_COMPLETE) {
        return 0;
    }
    printf("model=%s fact=%s maturity=%s transfers=%zu profile=%s\n",
        db_qualified_sequence_kind_name(sequence.kind), sequence.fact_id,
        sequence.evidence_maturity, sequence.role_count,
        db_transition_profile_name(
            db_transition_parser_observed_profile(&parser)));
    return 1;
}

static DBProtocolTransferMetadata
out_metadata(size_t length)
{
    return (DBProtocolTransferMetadata) {
        .direction = DB_PROTOCOL_DIRECTION_OUT,
        .endpoint = DB_TRAFFIC_REGIME_ENDPOINT_OUT,
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,
        .succeeded = 1U,
        .length = length
    };
}

static int
run_regime_windows(void)
{
    DBTrafficRegimeWindow window;
    DBTrafficRegimeState observed;
    db_traffic_regime_initialize(&window);
    DBProtocolTransferMetadata small = out_metadata(64U);
    if (db_traffic_regime_accept(&window, &small) != DB_TRAFFIC_REGIME_OK ||
        db_traffic_regime_finish(&window, &observed) !=
            DB_TRAFFIC_REGIME_OK ||
        observed != DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED) {
        return 0;
    }
    printf("window=%s inference=none\n",
        db_traffic_regime_state_name(observed));

    db_traffic_regime_initialize(&window);
    DBProtocolTransferMetadata larger = out_metadata(1088U);
    DBProtocolTransferMetadata largest = out_metadata(65536U);
    if (db_traffic_regime_accept(&window, &larger) !=
            DB_TRAFFIC_REGIME_OK ||
        db_traffic_regime_accept(&window, &largest) !=
            DB_TRAFFIC_REGIME_OK ||
        db_traffic_regime_finish(&window, &observed) !=
            DB_TRAFFIC_REGIME_OK ||
        observed != DB_TRAFFIC_REGIME_OUT_65536_OBSERVED) {
        return 0;
    }
    printf("window=%s inference=none\n",
        db_traffic_regime_state_name(observed));
    return 1;
}

int
main(void)
{
    LabFixture fixture;
    puts("DockBridge offline protocol-model lab");
    puts("Fake transport and source-authored bodies only; no device access.");
    puts("Observed OUT metadata is never treated as a request to replay.");
    if (!initialize_fixture(&fixture) ||
        !run_transition(&fixture,
            DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX,
            DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX, 0xa5U) ||
        !run_transition(&fixture,
            DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A, 0x3cU) ||
        !run_transition(&fixture,
            DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B, 0xa5U) ||
        !run_regime_windows()) {
        fputs("FAIL: offline model rejected its reviewed synthetic fixture.\n",
            stderr);
        return 1;
    }

    printf("fake-transport writes=%zu reads=%zu real-hardware-writes=0\n",
        fixture.fake.write_success_count, fixture.fake.read_success_count);
    if (fixture.fake.write_success_count != 32U ||
        fixture.fake.read_success_count != 50U ||
        fixture.transport.kind != DB_TRANSPORT_KIND_FAKE) {
        fputs("FAIL: fake transport counts or kind changed.\n", stderr);
        return 1;
    }
    puts("PASS: bounded models completed; hardware writes remain disabled.");
    return db_machine_detach(&fixture.machine) == DB_MACHINE_OK ? 0 : 1;
}
