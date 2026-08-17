#ifndef DOCKBRIDGE_EXCHANGE_PARSER_H
#define DOCKBRIDGE_EXCHANGE_PARSER_H

#include <stddef.h>
#include <stdint.h>

enum {
    DB_EXCHANGE_ENDPOINT_OUT = 0x02,
    DB_EXCHANGE_ENDPOINT_IN = 0x84,
    DB_EXCHANGE_IN_HEADER_SIZE = 4,
    DB_EXCHANGE_MAX_FRAME_SIZE = 1024,
    DB_EXCHANGE_PHASE_A_FRAME_COUNT = 15,
    DB_EXCHANGE_MAX_VARIABLE_RANGES_PER_ROLE = 2,
    DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX = 9,
    DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX = 10
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

typedef enum {
    DB_EXCHANGE_ORDER_UNDECIDED = 0,
    DB_EXCHANGE_ORDER_CANONICAL,
    DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED
} DBExchangeOrderVariant;

typedef struct {
    size_t offset;
    size_t length;
} DBExchangeObservedVariableRange;

/*
 * Canonical role-index metadata and its currently reviewed byte-position
 * correlation. A zero range count means no variation was observed; it does not
 * prove a protocol constant. Observed transfer order is tracked separately.
 */
typedef struct {
    DBExchangeDirection direction;
    size_t length;
    size_t observed_variable_range_count;
    DBExchangeObservedVariableRange observed_variable_ranges[
        DB_EXCHANGE_MAX_VARIABLE_RANGES_PER_ROLE];
} DBExchangeTransferStructure;

typedef struct {
    DBExchangeState state;
    /* Accepted USB transfers, not a video-frame or canonical-role index. */
    size_t next_frame_index;
    DBExchangeOrderVariant order_variant;
    uint8_t swapped_pair_pending;
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
const char *db_exchange_order_variant_name(DBExchangeOrderVariant variant);

#endif
