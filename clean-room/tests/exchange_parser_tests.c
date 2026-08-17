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

static uint8_t
frame_endpoint(const TestFrame *frame)
{
    return frame->direction == DB_EXCHANGE_DIRECTION_OUT ?
        DB_EXCHANGE_ENDPOINT_OUT : DB_EXCHANGE_ENDPOINT_IN;
}

static void
accept_role(DBExchangeParser *parser, uint8_t *bytes, size_t role_index)
{
    assert(role_index < DB_EXCHANGE_PHASE_A_FRAME_COUNT);
    make_frame(bytes, &phase_a[role_index]);
    assert(db_exchange_parser_accept(parser, phase_a[role_index].direction,
        frame_endpoint(&phase_a[role_index]), bytes,
        phase_a[role_index].length) == DB_EXCHANGE_OK);
}

int
main(void)
{
    uint8_t bytes[DB_EXCHANGE_MAX_FRAME_SIZE] = {0};
    DBExchangeParser parser = {0};
    db_exchange_parser_initialize(&parser);
    assert(parser.state == DB_EXCHANGE_WAITING);
    assert(parser.next_frame_index == 0);
    assert(parser.order_variant == DB_EXCHANGE_ORDER_UNDECIDED);
    assert(parser.swapped_pair_pending == 0);

    size_t observed_variable_ranges = 0;
    size_t observed_variable_bytes = 0;
    for (size_t index = 0; index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++index) {
        DBExchangeTransferStructure structure = {0};
        assert(db_exchange_phase_a_structure(index, &structure) ==
            DB_EXCHANGE_OK);
        assert(structure.direction == phase_a[index].direction);
        assert(structure.length == phase_a[index].length);
        assert(structure.observed_variable_range_count <=
            DB_EXCHANGE_MAX_VARIABLE_RANGES_PER_ROLE);
        size_t previous_range_end = 0;
        for (size_t range_index = 0;
             range_index < structure.observed_variable_range_count;
             ++range_index) {
            const DBExchangeObservedVariableRange *range =
                &structure.observed_variable_ranges[range_index];
            assert(range->length > 0);
            assert(range->offset <= structure.length);
            assert(range->length <= structure.length - range->offset);
            if (range_index > 0) {
                assert(previous_range_end < range->offset);
            }
            previous_range_end = range->offset + range->length;
            ++observed_variable_ranges;
            observed_variable_bytes += range->length;
        }
        for (size_t range_index = structure.observed_variable_range_count;
             range_index < DB_EXCHANGE_MAX_VARIABLE_RANGES_PER_ROLE;
             ++range_index) {
            assert(structure.observed_variable_ranges[range_index].offset == 0);
            assert(structure.observed_variable_ranges[range_index].length == 0);
        }
        if (index == 6) {
            assert(structure.observed_variable_range_count == 1);
            assert(structure.observed_variable_ranges[0].offset == 44);
            assert(structure.observed_variable_ranges[0].length == 8);
        } else if (index == 9) {
            assert(structure.observed_variable_range_count == 1);
            assert(structure.observed_variable_ranges[0].offset == 12);
            assert(structure.observed_variable_ranges[0].length == 1);
        } else if (index == 10) {
            assert(structure.observed_variable_range_count == 2);
            assert(structure.observed_variable_ranges[0].offset == 12);
            assert(structure.observed_variable_ranges[0].length == 1);
            assert(structure.observed_variable_ranges[1].offset == 24);
            assert(structure.observed_variable_ranges[1].length == 1);
        } else if (index == 12) {
            assert(structure.observed_variable_range_count == 1);
            assert(structure.observed_variable_ranges[0].offset == 44);
            assert(structure.observed_variable_ranges[0].length == 128);
        } else if (index == 14) {
            assert(structure.observed_variable_range_count == 1);
            assert(structure.observed_variable_ranges[0].offset == 26);
            assert(structure.observed_variable_ranges[0].length == 8);
        } else {
            assert(structure.observed_variable_range_count == 0);
        }
    }
    assert(observed_variable_ranges == 6);
    assert(observed_variable_bytes == 147);
    DBExchangeTransferStructure structure = {0};
    assert(db_exchange_phase_a_structure(DB_EXCHANGE_PHASE_A_FRAME_COUNT,
        &structure) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(db_exchange_phase_a_structure(0, NULL) ==
        DB_EXCHANGE_INVALID_ARGUMENT);

    for (size_t index = 0; index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++index) {
        accept_role(&parser, bytes, index);
        if (index < DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX) {
            assert(parser.order_variant == DB_EXCHANGE_ORDER_UNDECIDED);
        }
        if (index == DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX) {
            assert(parser.order_variant == DB_EXCHANGE_ORDER_CANONICAL);
            assert(parser.swapped_pair_pending == 0);
        }
    }
    assert(parser.state == DB_EXCHANGE_COMPLETE);
    assert(parser.next_frame_index == DB_EXCHANGE_PHASE_A_FRAME_COUNT);
    assert(parser.order_variant == DB_EXCHANGE_ORDER_CANONICAL);
    assert(parser.swapped_pair_pending == 0);
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_ALREADY_COMPLETE);

    db_exchange_parser_initialize(&parser);
    for (size_t observed_index = 0;
         observed_index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++observed_index) {
        size_t role_index = observed_index;
        if (observed_index == DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX) {
            role_index = DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX;
        } else if (observed_index == DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX) {
            role_index = DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX;
        }
        accept_role(&parser, bytes, role_index);
        if (observed_index == DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX) {
            assert(parser.order_variant ==
                DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED);
            assert(parser.swapped_pair_pending == 1);
        }
        if (observed_index == DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX) {
            assert(parser.order_variant ==
                DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED);
            assert(parser.swapped_pair_pending == 0);
        }
    }
    assert(parser.state == DB_EXCHANGE_COMPLETE);
    assert(parser.next_frame_index == DB_EXCHANGE_PHASE_A_FRAME_COUNT);
    assert(parser.order_variant == DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED);
    assert(parser.swapped_pair_pending == 0);

    db_exchange_parser_initialize(&parser);
    for (size_t index = 0; index < DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX;
         ++index) {
        accept_role(&parser, bytes, index);
    }
    accept_role(&parser, bytes, DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX);
    assert(parser.swapped_pair_pending == 1);
    make_frame(bytes, &phase_a[11]);
    assert(db_exchange_parser_accept(&parser, phase_a[11].direction,
        frame_endpoint(&phase_a[11]), bytes, phase_a[11].length) ==
        DB_EXCHANGE_UNEXPECTED_FRAME);
    assert(parser.state == DB_EXCHANGE_FAILED);
    assert(parser.next_frame_index == DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX);
    assert(parser.order_variant == DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED);
    assert(parser.swapped_pair_pending == 1);

    db_exchange_parser_initialize(&parser);
    for (size_t index = 0; index <= DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX;
         ++index) {
        accept_role(&parser, bytes, index);
    }
    make_frame(bytes, &phase_a[DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX]);
    assert(db_exchange_parser_accept(&parser,
        phase_a[DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX].direction,
        frame_endpoint(&phase_a[DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX]), bytes,
        phase_a[DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX].length) ==
        DB_EXCHANGE_UNEXPECTED_FRAME);
    assert(parser.state == DB_EXCHANGE_FAILED);
    assert(parser.next_frame_index == DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX);
    assert(parser.order_variant == DB_EXCHANGE_ORDER_CANONICAL);
    assert(parser.swapped_pair_pending == 0);

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
        .state = DB_EXCHANGE_WAITING,
        .order_variant = DB_EXCHANGE_ORDER_CANONICAL
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_OUT,
        DB_EXCHANGE_ENDPOINT_OUT, bytes, 16) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_IN_PROGRESS,
        .next_frame_index = DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX,
        .order_variant = DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 38) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_IN_PROGRESS,
        .next_frame_index = DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX,
        .order_variant = DB_EXCHANGE_ORDER_CANONICAL,
        .swapped_pair_pending = 1
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 549) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_IN_PROGRESS,
        .next_frame_index = DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX + 1U,
        .order_variant = DB_EXCHANGE_ORDER_UNDECIDED
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 31) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_IN_PROGRESS,
        .next_frame_index = DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX + 1U,
        .order_variant = (DBExchangeOrderVariant)99
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 31) == DB_EXCHANGE_INVALID_ARGUMENT);
    assert(parser.state == DB_EXCHANGE_FAILED);
    parser = (DBExchangeParser) {
        .state = DB_EXCHANGE_IN_PROGRESS,
        .next_frame_index = DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX,
        .order_variant = DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED,
        .swapped_pair_pending = 2
    };
    assert(db_exchange_parser_accept(&parser, DB_EXCHANGE_DIRECTION_IN,
        DB_EXCHANGE_ENDPOINT_IN, bytes, 38) == DB_EXCHANGE_INVALID_ARGUMENT);
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
    assert(strcmp(db_exchange_order_variant_name(DB_EXCHANGE_ORDER_CANONICAL),
        "canonical") == 0);
    assert(strcmp(db_exchange_order_variant_name(
        DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED),
        "canonical-role-indices-9-10-swapped") == 0);
    assert(strcmp(db_exchange_state_name((DBExchangeState)99),
        "invalid-state") == 0);
    assert(strcmp(db_exchange_result_name((DBExchangeResult)99),
        "invalid-result") == 0);
    assert(strcmp(db_exchange_order_variant_name((DBExchangeOrderVariant)99),
        "invalid-order-variant") == 0);
    return 0;
}
