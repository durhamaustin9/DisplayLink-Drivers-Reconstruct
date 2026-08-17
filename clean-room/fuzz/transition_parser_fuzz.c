#include "fake_transport.h"
#include "qualified_sequences.h"
#include "transition_parser.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    DB_FUZZ_RECORD_SIZE = 8,
    DB_FUZZ_MAX_RECORDS = 40,
    DB_FUZZ_STANDALONE_CASES = 2048,
    DB_FUZZ_STANDALONE_INPUT_SIZE = 256
};

typedef struct {
    DBFakeTransport fake;
    DBTransport transport;
    DBMachine machine;
} FuzzFixture;

static int
initialize_fixture(FuzzFixture *fixture)
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
            DB_MACHINE_OK;
}

static DBProtocolTransferMetadata
decode_record(const uint8_t *record)
{
    DBProtocolDirection direction = (record[0] & 1U) != 0U ?
        DB_PROTOCOL_DIRECTION_IN : DB_PROTOCOL_DIRECTION_OUT;
    size_t length = (size_t)record[4] |
        ((size_t)record[5] << 8U);
    if ((record[6] & 1U) != 0U) {
        ++length;
    }
    DBProtocolTransferMetadata metadata = {
        .direction = direction,
        .endpoint = (record[1] & 1U) != 0U ?
            (direction == DB_PROTOCOL_DIRECTION_IN ? 0x84U : 0x02U) :
            record[2],
        .kind = (record[3] & 1U) != 0U ?
            DB_PROTOCOL_TRANSFER_KIND_BULK :
            DB_PROTOCOL_TRANSFER_KIND_INVALID,
        .succeeded = (uint8_t)((record[3] >> 1U) & 3U),
        .length = length
    };
    if (direction == DB_PROTOCOL_DIRECTION_IN && length >= 4U) {
        size_t body_length = length - 4U;
        metadata.in_prefix[2] = (uint8_t)(body_length & 0xffU);
        metadata.in_prefix[3] = (uint8_t)((body_length >> 8U) & 0xffU);
        if ((record[6] & 2U) != 0U) {
            metadata.in_prefix[0] = 1U;
        }
        if ((record[6] & 4U) != 0U) {
            metadata.in_prefix[3] ^= 1U;
        }
    } else if (direction == DB_PROTOCOL_DIRECTION_OUT &&
        (record[6] & 8U) != 0U) {
        metadata.in_prefix[0] = 1U;
    }
    return metadata;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == NULL || size == 0U) {
        return 0;
    }
    FuzzFixture fixture;
    if (!initialize_fixture(&fixture)) {
        return 0;
    }
    DBTransitionKind kind = (data[0] & 1U) != 0U ?
        DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE :
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX;
    DBTransitionParser parser = {0};
    if (db_transition_parser_initialize(&parser, kind, &fixture.machine) !=
        DB_TRANSITION_RESULT_OK) {
        (void)db_machine_detach(&fixture.machine);
        return 0;
    }

    size_t record_count = (size - 1U) / DB_FUZZ_RECORD_SIZE;
    if (record_count > DB_FUZZ_MAX_RECORDS) {
        record_count = DB_FUZZ_MAX_RECORDS;
    }
    for (size_t index = 0; index < record_count; ++index) {
        const uint8_t *record = data + 1U + index * DB_FUZZ_RECORD_SIZE;
        if (index == 0U && (record[7] & 0x80U) != 0U) {
            parser.accepted_count = (size_t)record[4] |
                ((size_t)record[5] << 8U);
        }
        if ((record[7] & 0x40U) != 0U) {
            (void)db_machine_detach(&fixture.machine);
        }
        DBProtocolTransferMetadata metadata = decode_record(record);
        DBTransitionResult result = db_transition_parser_accept(&parser,
            &metadata);
        if (result == DB_TRANSITION_RESULT_FAILED ||
            parser.state == DB_TRANSITION_STATE_FAILED ||
            result == DB_TRANSITION_RESULT_COMPLETE ||
            result == DB_TRANSITION_RESULT_ALREADY_COMPLETE) {
            break;
        }
    }
    (void)db_transition_parser_finish(&parser);
    assert(fixture.fake.write_attempt_count == 0U);
    assert(fixture.fake.read_attempt_count == 0U);
    if (parser.state != DB_TRANSITION_STATE_FAILED) {
        assert(parser.accepted_count <=
            DB_TRANSITION_HOTUNPLUG_PROFILE_ROLE_COUNT);
        assert(parser.active_profile_mask != 0U);
    }
    (void)db_machine_detach(&fixture.machine);
    return 0;
}

#ifdef DB_FUZZ_STANDALONE
static uint32_t
next_random(uint32_t *state)
{
    uint32_t value = *state;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    *state = value;
    return value;
}

static void
run_valid_seed(DBTransitionKind parser_kind,
    DBQualifiedSequenceKind sequence_kind)
{
    FuzzFixture fixture;
    assert(initialize_fixture(&fixture));
    DBTransitionParser parser = {0};
    DBQualifiedSequence sequence = {0};
    assert(db_qualified_sequence_get(sequence_kind, &sequence));
    assert(db_transition_parser_initialize(&parser, parser_kind,
        &fixture.machine) == DB_TRANSITION_RESULT_OK);
    for (size_t index = 0; index < sequence.role_count; ++index) {
        const DBPartialOrderRole *role = &sequence.roles[index];
        DBProtocolTransferMetadata metadata = {
            .direction = role->direction,
            .endpoint = role->endpoint,
            .kind = role->kind,
            .succeeded = role->required_succeeded,
            .length = role->allowed_lengths[0]
        };
        if (metadata.direction == DB_PROTOCOL_DIRECTION_IN) {
            size_t body_length = metadata.length - 4U;
            metadata.in_prefix[2] = (uint8_t)(body_length & 0xffU);
            metadata.in_prefix[3] =
                (uint8_t)((body_length >> 8U) & 0xffU);
        }
        DBTransitionResult result = db_transition_parser_accept(&parser,
            &metadata);
        assert(result == (index + 1U == sequence.role_count ?
            DB_TRANSITION_RESULT_COMPLETE : DB_TRANSITION_RESULT_OK));
    }
    assert(db_transition_parser_finish(&parser) ==
        DB_TRANSITION_RESULT_COMPLETE);
    assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
}

static size_t
parse_case_count(const char *text)
{
    if (text == NULL || *text == '\0') {
        return 0U;
    }
    size_t value = 0U;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9' ||
            value > (SIZE_MAX - (size_t)(*cursor - '0')) / 10U) {
            return 0U;
        }
        value = value * 10U + (size_t)(*cursor - '0');
    }
    return value;
}

int
main(int argc, char **argv)
{
    run_valid_seed(DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX,
        DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX);
    run_valid_seed(DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
        DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A);
    run_valid_seed(DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
        DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B);

    size_t case_count = DB_FUZZ_STANDALONE_CASES;
    if (argc == 2) {
        case_count = parse_case_count(argv[1]);
        if (case_count == 0U) {
            return 64;
        }
    } else if (argc != 1) {
        return 64;
    }

    uint32_t random_state = UINT32_C(0x6d2b79f5);
    uint8_t input[DB_FUZZ_STANDALONE_INPUT_SIZE];
    for (size_t test_case = 0; test_case < case_count;
         ++test_case) {
        for (size_t index = 0; index < sizeof(input); ++index) {
            input[index] = (uint8_t)next_random(&random_state);
        }
        size_t length = 1U +
            (next_random(&random_state) % (sizeof(input) - 1U));
        (void)LLVMFuzzerTestOneInput(input, length);
    }
    return 0;
}
#endif
