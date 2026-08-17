#include "exchange_parser.h"

#include <assert.h>
#include <string.h>

typedef struct {
    DBExchangeDirection direction;
    size_t length;
} TestFrame;

static const TestFrame phase_a[DB_EXCHANGE_PHASE_A_FRAME_COUNT] = {
    {DB_EXCHANGE_DIRECTION_OUT, 16},
    {DB_EXCHANGE_DIRECTION_OUT, 32},
    {DB_EXCHANGE_DIRECTION_OUT, 80},
    {DB_EXCHANGE_DIRECTION_IN, 39},
    {DB_EXCHANGE_DIRECTION_OUT, 48},
    {DB_EXCHANGE_DIRECTION_IN, 38},
    {DB_EXCHANGE_DIRECTION_OUT, 64},
    {DB_EXCHANGE_DIRECTION_IN, 38},
    {DB_EXCHANGE_DIRECTION_OUT, 64},
    {DB_EXCHANGE_DIRECTION_IN, 38},
    {DB_EXCHANGE_DIRECTION_IN, 549},
    {DB_EXCHANGE_DIRECTION_IN, 31},
    {DB_EXCHANGE_DIRECTION_OUT, 176},
    {DB_EXCHANGE_DIRECTION_IN, 38},
    {DB_EXCHANGE_DIRECTION_IN, 34}
};

static void
make_frame(uint8_t *bytes, const TestFrame *frame)
{
    memset(bytes, 0xa5, frame->length);
    if (frame->direction == DB_EXCHANGE_DIRECTION_IN) {
        size_t body = frame->length - DB_EXCHANGE_IN_HEADER_SIZE;
        bytes[0] = 0;
        bytes[1] = 0;
        bytes[2] = (uint8_t)(body & 0xffU);
        bytes[3] = (uint8_t)((body >> 8U) & 0xffU);
    }
}

int
main(void)
{
    uint8_t bytes[DB_EXCHANGE_MAX_FRAME_SIZE] = {0};
    DBExchangeParser parser = {0};
    db_exchange_parser_initialize(&parser);
    assert(parser.state == DB_EXCHANGE_WAITING);
    assert(parser.next_frame_index == 0);

    size_t observed_variable_windows = 0;
    size_t observed_variable_bytes = 0;
    for (size_t index = 0; index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++index) {
        DBExchangeTransferStructure structure = {0};
        assert(db_exchange_phase_a_structure(index, &structure) ==
            DB_EXCHANGE_OK);
        assert(structure.direction == phase_a[index].direction);
        assert(structure.length == phase_a[index].length);
        assert(structure.observed_variable_offset +
            structure.observed_variable_length <= structure.length);
        if (structure.observed_variable_length > 0) {
            ++observed_variable_windows;
            observed_variable_bytes += structure.observed_variable_length;
        } else {
            assert(structure.observed_variable_offset == 0);
        }
        if (index == 6) {
            assert(structure.observed_variable_offset == 44);
            assert(structure.observed_variable_length == 8);
        } else if (index == 10) {
            assert(structure.observed_variable_offset == 24);
            assert(structure.observed_variable_length == 1);
        } else if (index == 12) {
            assert(structure.observed_variable_offset == 44);
            assert(structure.observed_variable_length == 128);
        } else if (index == 14) {
            assert(structure.observed_variable_offset == 26);
            assert(structure.observed_variable_length == 8);
        }
    }
    assert(observed_variable_windows == 4);
    assert(observed_variable_bytes == 145);
    DBExchangeTransferStructure structure = {0};
    assert(db_exchange_phase_a_structure(DB_EXCHANGE_PHASE_A_FRAME_COUNT,
        &structure) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(db_exchange_phase_a_structure(0, NULL) ==
        DB_EXCHANGE_INVALID_ARGUMENT);

    for (size_t index = 0; index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++index) {
        make_frame(bytes, &phase_a[index]);
        assert(db_exchange_parser_accept(&parser, phase_a[index].direction,
            phase_a[index].direction == DB_EXCHANGE_DIRECTION_OUT ?
                DB_EXCHANGE_ENDPOINT_OUT : DB_EXCHANGE_ENDPOINT_IN,
            bytes, phase_a[index].length) == DB_EXCHANGE_OK);
    }
    assert(parser.state == DB_EXCHANGE_COMPLETE);
    assert(parser.next_frame_index == DB_EXCHANGE_PHASE_A_FRAME_COUNT);
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_ALREADY_COMPLETE);

    db_exchange_parser_initialize(&parser);
    memset(bytes, 0, sizeof(bytes));
    TestFrame valid_but_unexpected = {DB_EXCHANGE_DIRECTION_IN, 39};
    make_frame(bytes, &valid_but_unexpected);
    assert(db_exchange_parser_accept(&parser, valid_but_unexpected.direction,
        DB_EXCHANGE_ENDPOINT_IN, bytes, valid_but_unexpected.length) ==
        DB_EXCHANGE_UNEXPECTED_FRAME);
    assert(parser.state == DB_EXCHANGE_FAILED);

    db_exchange_parser_initialize(&parser);
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 32) == DB_EXCHANGE_UNEXPECTED_FRAME);
    assert(parser.state == DB_EXCHANGE_FAILED);

    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 15) == DB_EXCHANGE_INVALID_FRAME);
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_OK);
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 16) == DB_EXCHANGE_INVALID_FRAME);
    bytes[2] = 12;
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 16) == DB_EXCHANGE_OK);
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_INVALID_FRAME);
    bytes[0] = 1;
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 16) == DB_EXCHANGE_INVALID_FRAME);
    bytes[0] = 0;
    bytes[2] = 11;
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 16) == DB_EXCHANGE_INVALID_FRAME);
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, NULL, 16) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 0) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, DB_EXCHANGE_MAX_FRAME_SIZE + 1U) ==
        DB_EXCHANGE_INVALID_ARGUMENT);
    assert(db_exchange_validate_frame((DBExchangeDirection)99,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(db_exchange_parser_accept(NULL, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_INVALID_ARGUMENT);

    TestFrame longest_in = {DB_EXCHANGE_DIRECTION_IN, 549};
    make_frame(bytes, &longest_in);
    assert(bytes[2] == 0x21 && bytes[3] == 0x02);
    assert(db_exchange_validate_frame(DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, longest_in.length) == DB_EXCHANGE_OK);

    db_exchange_parser_initialize(&parser);
    for (size_t index = 0; index < 3; ++index) {
        make_frame(bytes, &phase_a[index]);
        assert(db_exchange_parser_accept(&parser, phase_a[index].direction,
            DB_EXCHANGE_ENDPOINT_OUT, bytes, phase_a[index].length) ==
            DB_EXCHANGE_OK);
    }
    make_frame(bytes, &phase_a[3]);
    ++bytes[2];
    assert(db_exchange_parser_accept(&parser, phase_a[3].direction,
        DB_EXCHANGE_ENDPOINT_IN, bytes, phase_a[3].length) ==
        DB_EXCHANGE_INVALID_FRAME);
    assert(parser.state == DB_EXCHANGE_FAILED);
    assert(parser.next_frame_index == 3);
    make_frame(bytes, &phase_a[3]);
    assert(db_exchange_parser_accept(&parser, phase_a[3].direction,
        DB_EXCHANGE_ENDPOINT_IN, bytes, phase_a[3].length) ==
        DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    assert(parser.next_frame_index == 3);

    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_WAITING,
        .next_frame_index = SIZE_MAX
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_FAILED,
        .next_frame_index = SIZE_MAX
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    assert(parser.next_frame_index == SIZE_MAX);
    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_IN_PROGRESS,
        .next_frame_index = 0
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_COMPLETE,
        .next_frame_index = DB_EXCHANGE_PHASE_A_FRAME_COUNT - 1U
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    assert(strcmp(db_exchange_state_name(DB_EXCHANGE_COMPLETE),
        "complete") == 0);
    assert(strcmp(db_exchange_result_name(DB_EXCHANGE_UNEXPECTED_FRAME),
        "unexpected-frame") == 0);
    assert(strcmp(db_exchange_state_name((DBExchangeState)99),
        "invalid-state") == 0);
    assert(strcmp(db_exchange_result_name((DBExchangeResult)99),
        "invalid-result") == 0);
    return 0;
}
