#ifndef DOCKBRIDGE_EXCHANGE_PARSER_H
#define DOCKBRIDGE_EXCHANGE_PARSER_H

#include <stddef.h>
#include <stdint.h>

enum {
    DB_EXCHANGE_ENDPOINT_OUT = 0x02,
    DB_EXCHANGE_ENDPOINT_IN = 0x84,
    DB_EXCHANGE_IN_HEADER_SIZE = 4,
    DB_EXCHANGE_MAX_FRAME_SIZE = 1024,
    DB_EXCHANGE_PHASE_A_FRAME_COUNT = 15
};

typedef enum {
    DB_EXCHANGE_DIRECTION_OUT = 1,
    DB_EXCHANGE_DIRECTION_IN
} DBExchangeDirection;

typedef enum {
    DB_EXCHANGE_WAITING = 0,
    DB_EXCHANGE_IN_PROGRESS,
    DB_EXCHANGE_COMPLETE,
    DB_EXCHANGE_FAILED
} DBExchangeState;

typedef enum {
    DB_EXCHANGE_OK = 0,
    DB_EXCHANGE_INVALID_ARGUMENT,
    DB_EXCHANGE_INVALID_FRAME,
    DB_EXCHANGE_UNEXPECTED_FRAME,
    DB_EXCHANGE_ALREADY_COMPLETE
} DBExchangeResult;

/*
 * Byte-position correlation across five controlled observations, including
 * monitor-absent and reported full-power-cycle controls. A zero variable length
 * means no variation was observed; it does not prove a protocol constant.
 */
typedef struct {
    DBExchangeDirection direction;
    size_t length;
    size_t observed_variable_offset;
    size_t observed_variable_length;
} DBExchangeTransferStructure;

typedef struct {
    DBExchangeState state;
    size_t next_frame_index;
} DBExchangeParser;

void db_exchange_parser_initialize(DBExchangeParser *parser);
DBExchangeResult db_exchange_phase_a_structure(size_t transfer_index,
    DBExchangeTransferStructure *structure);
DBExchangeResult db_exchange_validate_frame(DBExchangeDirection direction,
    uint8_t endpoint, const uint8_t *bytes, size_t length);
DBExchangeResult db_exchange_parser_accept(DBExchangeParser *parser,
    DBExchangeDirection direction, uint8_t endpoint, const uint8_t *bytes,
    size_t length);
const char *db_exchange_state_name(DBExchangeState state);
const char *db_exchange_result_name(DBExchangeResult result);

#endif
