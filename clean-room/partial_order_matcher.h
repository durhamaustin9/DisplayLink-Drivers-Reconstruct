#ifndef DOCKBRIDGE_PARTIAL_ORDER_MATCHER_H
#define DOCKBRIDGE_PARTIAL_ORDER_MATCHER_H

#include "protocol_transfer.h"

#include <stddef.h>
#include <stdint.h>

enum {
    DB_PARTIAL_ORDER_MAX_ROLES = 32,
    DB_PARTIAL_ORDER_MAX_ALLOWED_LENGTHS = 4,
    DB_PARTIAL_ORDER_MAX_CANDIDATES = 8
};

typedef enum {
    DB_PARTIAL_ORDER_STATE_WAITING = 0,
    DB_PARTIAL_ORDER_STATE_IN_PROGRESS,
    DB_PARTIAL_ORDER_STATE_COMPLETE,
    DB_PARTIAL_ORDER_STATE_FAILED
} DBPartialOrderState;

typedef enum {
    DB_PARTIAL_ORDER_RESULT_OK = 0,
    DB_PARTIAL_ORDER_RESULT_COMPLETE,
    DB_PARTIAL_ORDER_RESULT_INCOMPLETE,
    DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT,
    DB_PARTIAL_ORDER_RESULT_INVALID_MODEL,
    DB_PARTIAL_ORDER_RESULT_INVALID_TRANSFER,
    DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER,
    DB_PARTIAL_ORDER_RESULT_CANDIDATE_LIMIT,
    DB_PARTIAL_ORDER_RESULT_CORRUPT_STATE,
    DB_PARTIAL_ORDER_RESULT_FAILED,
    DB_PARTIAL_ORDER_RESULT_ALREADY_COMPLETE
} DBPartialOrderResult;

/*
 * One role in a finite partial order. predecessor_mask names roles that must
 * already be complete. Allowed lengths must be strictly increasing; unused
 * entries must be zero. All other transfer metadata is matched exactly.
 */
typedef struct {
    DBProtocolDirection direction;
    uint8_t endpoint;
    DBProtocolTransferKind kind;
    uint8_t required_succeeded;
    uint8_t allowed_length_count;
    size_t allowed_lengths[DB_PARTIAL_ORDER_MAX_ALLOWED_LENGTHS];
    uint32_t predecessor_mask;
} DBPartialOrderRole;

/*
 * Fixed storage makes ownership and resource limits explicit. Candidate masks
 * form a bounded nondeterministic frontier, so identical eligible roles are
 * retained until later metadata resolves the ambiguity instead of being
 * selected greedily.
 */
typedef struct {
    DBPartialOrderState state;
    size_t role_count;
    size_t consumed_transfer_count;
    size_t candidate_count;
    uint32_t all_roles_mask;
    uint32_t candidate_masks[DB_PARTIAL_ORDER_MAX_CANDIDATES];
    DBPartialOrderRole roles[DB_PARTIAL_ORDER_MAX_ROLES];
} DBPartialOrderMatcher;

DBPartialOrderResult db_partial_order_matcher_initialize(
    DBPartialOrderMatcher *matcher, const DBPartialOrderRole *roles,
    size_t role_count);
DBPartialOrderResult db_partial_order_matcher_accept(
    DBPartialOrderMatcher *matcher,
    const DBProtocolTransferMetadata *transfer);

/*
 * COMPLETE is idempotent. Finishing a waiting or in-progress matcher returns
 * INCOMPLETE and puts it in the sticky FAILED state.
 */
DBPartialOrderResult db_partial_order_matcher_finish(
    DBPartialOrderMatcher *matcher);

const char *db_partial_order_state_name(DBPartialOrderState state);
const char *db_partial_order_result_name(DBPartialOrderResult result);

#endif
