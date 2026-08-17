#include "exchange_parser.h"

/*
 * Externally observed phase-A transfer shape. The table contains no captured
 * bytes and assigns no command meaning to any transfer. Variable ranges are
 * role-aligned byte-position correlations, not interpreted fields.
 */
static const DBExchangeTransferStructure
phase_a_shape[DB_EXCHANGE_PHASE_A_FRAME_COUNT] = {
    {DB_EXCHANGE_DIRECTION_OUT, 16, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_OUT, 32, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_OUT, 80, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_IN, 39, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_OUT, 48, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_IN, 38, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_OUT, 64, 1, {{44, 8}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_IN, 38, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_OUT, 64, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_IN, 38, 1, {{12, 1}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_IN, 549, 2, {{12, 1}, {24, 1}}},
    {DB_EXCHANGE_DIRECTION_IN, 31, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_OUT, 176, 1, {{44, 128}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_IN, 38, 0, {{0, 0}, {0, 0}}},
    {DB_EXCHANGE_DIRECTION_IN, 34, 1, {{26, 8}, {0, 0}}}
};

static int
order_variant_is_valid(DBExchangeOrderVariant variant)
{
    switch (variant) {
    case DB_EXCHANGE_ORDER_UNDECIDED:
    case DB_EXCHANGE_ORDER_CANONICAL:
    case DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED:
        return 1;
    }
    return 0;
}

static int
parser_progress_is_consistent(const DBExchangeParser *parser)
{
    if (parser->next_frame_index > DB_EXCHANGE_PHASE_A_FRAME_COUNT ||
        parser->swapped_pair_pending > 1U ||
        !order_variant_is_valid(parser->order_variant)) {
        return 0;
    }
    if (parser->next_frame_index <= DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX) {
        return parser->order_variant == DB_EXCHANGE_ORDER_UNDECIDED &&
            parser->swapped_pair_pending == 0U;
    }
    if (parser->next_frame_index == DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX) {
        return (parser->order_variant == DB_EXCHANGE_ORDER_CANONICAL &&
                   parser->swapped_pair_pending == 0U) ||
            (parser->order_variant ==
                    DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED &&
                parser->swapped_pair_pending == 1U);
    }
    return parser->order_variant != DB_EXCHANGE_ORDER_UNDECIDED &&
        parser->swapped_pair_pending == 0U;
}

static int
parser_is_consistent(const DBExchangeParser *parser)
{
    if (!parser_progress_is_consistent(parser)) {
        return 0;
    }
    switch (parser->state) {
    case DB_EXCHANGE_WAITING:
        return parser->next_frame_index == 0;
    case DB_EXCHANGE_IN_PROGRESS:
        return parser->next_frame_index > 0 &&
            parser->next_frame_index < DB_EXCHANGE_PHASE_A_FRAME_COUNT;
    case DB_EXCHANGE_COMPLETE:
        return parser->next_frame_index == DB_EXCHANGE_PHASE_A_FRAME_COUNT;
    case DB_EXCHANGE_FAILED:
        return 1;
    }
    return 0;
}

static int
frame_matches_role(size_t role_index, DBExchangeDirection direction,
    size_t length)
{
    if (role_index >= DB_EXCHANGE_PHASE_A_FRAME_COUNT) {
        return 0;
    }
    const DBExchangeTransferStructure *role = &phase_a_shape[role_index];
    return direction == role->direction && length == role->length;
}

void
db_exchange_parser_initialize(DBExchangeParser *parser)
{
    if (parser != NULL) {
        *parser = (DBExchangeParser) {
            .state = DB_EXCHANGE_WAITING
        };
    }
}

DBExchangeResult
db_exchange_phase_a_structure(size_t transfer_index,
    DBExchangeTransferStructure *structure)
{
    if (structure == NULL ||
        transfer_index >= DB_EXCHANGE_PHASE_A_FRAME_COUNT) {
        return DB_EXCHANGE_INVALID_ARGUMENT;
    }
    *structure = phase_a_shape[transfer_index];
    return DB_EXCHANGE_OK;
}

DBExchangeResult
db_exchange_validate_frame(DBExchangeDirection direction,
    uint8_t endpoint, const uint8_t *bytes, size_t length)
{
    if (bytes == NULL || length == 0 || length > DB_EXCHANGE_MAX_FRAME_SIZE ||
        (direction != DB_EXCHANGE_DIRECTION_OUT &&
            direction != DB_EXCHANGE_DIRECTION_IN)) {
        return DB_EXCHANGE_INVALID_ARGUMENT;
    }
    if (direction == DB_EXCHANGE_DIRECTION_OUT) {
        if (endpoint != DB_EXCHANGE_ENDPOINT_OUT) {
            return DB_EXCHANGE_INVALID_FRAME;
        }
        return length % 16U == 0 ? DB_EXCHANGE_OK :
            DB_EXCHANGE_INVALID_FRAME;
    }
    if (endpoint != DB_EXCHANGE_ENDPOINT_IN) {
        return DB_EXCHANGE_INVALID_FRAME;
    }
    if (length < DB_EXCHANGE_IN_HEADER_SIZE || bytes[0] != 0 || bytes[1] != 0) {
        return DB_EXCHANGE_INVALID_FRAME;
    }
    size_t declared_body_length = (size_t)bytes[2] | ((size_t)bytes[3] << 8U);
    return declared_body_length == length - DB_EXCHANGE_IN_HEADER_SIZE ?
        DB_EXCHANGE_OK : DB_EXCHANGE_INVALID_FRAME;
}

DBExchangeResult
db_exchange_parser_accept(DBExchangeParser *parser,
    DBExchangeDirection direction, uint8_t endpoint, const uint8_t *bytes,
    size_t length)
{
    if (parser == NULL) {
        return DB_EXCHANGE_INVALID_ARGUMENT;
    }
    if (!parser_is_consistent(parser) || parser->state == DB_EXCHANGE_FAILED) {
        parser->state = DB_EXCHANGE_FAILED;
        return DB_EXCHANGE_INVALID_ARGUMENT;
    }
    if (parser->state == DB_EXCHANGE_COMPLETE) {
        return DB_EXCHANGE_ALREADY_COMPLETE;
    }
    DBExchangeResult validation = db_exchange_validate_frame(
        direction, endpoint, bytes, length);
    if (validation != DB_EXCHANGE_OK) {
        parser->state = DB_EXCHANGE_FAILED;
        return validation;
    }
    size_t observed_index = parser->next_frame_index;
    size_t expected_role_index = observed_index;
    if (observed_index == DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX) {
        if (frame_matches_role(DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX,
                direction, length)) {
            parser->order_variant = DB_EXCHANGE_ORDER_CANONICAL;
        } else if (frame_matches_role(DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX,
                       direction, length)) {
            parser->order_variant =
                DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED;
            parser->swapped_pair_pending = 1U;
        } else {
            parser->state = DB_EXCHANGE_FAILED;
            return DB_EXCHANGE_UNEXPECTED_FRAME;
        }
    } else {
        if (observed_index == DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX &&
            parser->order_variant ==
                DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED) {
            expected_role_index = DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX;
        }
        if (!frame_matches_role(expected_role_index, direction, length)) {
            parser->state = DB_EXCHANGE_FAILED;
            return DB_EXCHANGE_UNEXPECTED_FRAME;
        }
        if (observed_index == DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX) {
            parser->swapped_pair_pending = 0U;
        }
    }

    ++parser->next_frame_index;
    if (parser->next_frame_index == DB_EXCHANGE_PHASE_A_FRAME_COUNT) {
        parser->state = DB_EXCHANGE_COMPLETE;
    } else {
        parser->state = DB_EXCHANGE_IN_PROGRESS;
    }
    return DB_EXCHANGE_OK;
}

const char *
db_exchange_state_name(DBExchangeState state)
{
    switch (state) {
    case DB_EXCHANGE_WAITING:
        return "waiting";
    case DB_EXCHANGE_IN_PROGRESS:
        return "in-progress";
    case DB_EXCHANGE_COMPLETE:
        return "complete";
    case DB_EXCHANGE_FAILED:
        return "failed";
    }
    return "invalid-state";
}

const char *
db_exchange_result_name(DBExchangeResult result)
{
    switch (result) {
    case DB_EXCHANGE_OK:
        return "ok";
    case DB_EXCHANGE_INVALID_ARGUMENT:
        return "invalid-argument";
    case DB_EXCHANGE_INVALID_FRAME:
        return "invalid-frame";
    case DB_EXCHANGE_UNEXPECTED_FRAME:
        return "unexpected-frame";
    case DB_EXCHANGE_ALREADY_COMPLETE:
        return "already-complete";
    }
    return "invalid-result";
}

const char *
db_exchange_order_variant_name(DBExchangeOrderVariant variant)
{
    switch (variant) {
    case DB_EXCHANGE_ORDER_UNDECIDED:
        return "undecided";
    case DB_EXCHANGE_ORDER_CANONICAL:
        return "canonical";
    case DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED:
        return "canonical-role-indices-9-10-swapped";
    }
    return "invalid-order-variant";
}
