#include "exchange_parser.h"

typedef struct {
    DBExchangeDirection direction;
    size_t length;
} DBExchangeShape;

/*
 * Externally observed phase-A transfer shape. The table contains no captured
 * bytes and assigns no command meaning to any frame.
 */
static const DBExchangeShape phase_a_shape[DB_EXCHANGE_PHASE_A_FRAME_COUNT] = {
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

static int
parser_is_consistent(const DBExchangeParser *parser)
{
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
    const DBExchangeShape *expected =
        &phase_a_shape[parser->next_frame_index];
    if (direction != expected->direction || length != expected->length) {
        parser->state = DB_EXCHANGE_FAILED;
        return DB_EXCHANGE_UNEXPECTED_FRAME;
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
