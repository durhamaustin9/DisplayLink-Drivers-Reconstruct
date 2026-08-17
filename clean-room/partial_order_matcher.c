#include "partial_order_matcher.h"

#include <limits.h>
#include <string.h>

static int
direction_is_valid(DBProtocolDirection direction)
{
    return direction == DB_PROTOCOL_DIRECTION_OUT ||
        direction == DB_PROTOCOL_DIRECTION_IN;
}

static int
kind_is_valid(DBProtocolTransferKind kind)
{
    return kind == DB_PROTOCOL_TRANSFER_KIND_BULK;
}

static int
endpoint_matches_direction(uint8_t endpoint, DBProtocolDirection direction)
{
    if ((endpoint & 0x70U) != 0U || (endpoint & 0x0fU) == 0U) {
        return 0;
    }
    return direction == DB_PROTOCOL_DIRECTION_IN ?
        (endpoint & 0x80U) != 0U : (endpoint & 0x80U) == 0U;
}

int
db_protocol_transfer_metadata_is_valid(
    const DBProtocolTransferMetadata *transfer)
{
    if (transfer == NULL || !direction_is_valid(transfer->direction) ||
        !kind_is_valid(transfer->kind) || transfer->succeeded > 1U ||
        transfer->length == 0U ||
        transfer->length > DB_PROTOCOL_MAX_TRANSFER_LENGTH ||
        !endpoint_matches_direction(transfer->endpoint,
            transfer->direction)) {
        return 0;
    }
    if (transfer->direction == DB_PROTOCOL_DIRECTION_OUT) {
        for (size_t index = 0; index < DB_PROTOCOL_IN_PREFIX_SIZE; ++index) {
            if (transfer->in_prefix[index] != 0U) {
                return 0;
            }
        }
        return 1;
    }
    if (transfer->length < DB_PROTOCOL_IN_PREFIX_SIZE ||
        transfer->in_prefix[0] != 0U || transfer->in_prefix[1] != 0U) {
        return 0;
    }
    size_t declared_length = (size_t)transfer->in_prefix[2] |
        ((size_t)transfer->in_prefix[3] << 8U);
    return declared_length == transfer->length - DB_PROTOCOL_IN_PREFIX_SIZE;
}

static uint32_t
roles_mask(size_t role_count)
{
    return role_count == DB_PARTIAL_ORDER_MAX_ROLES ? UINT32_MAX :
        (UINT32_C(1) << role_count) - UINT32_C(1);
}

static size_t
mask_population(uint32_t mask)
{
    size_t count = 0;
    while (mask != 0U) {
        mask &= mask - UINT32_C(1);
        ++count;
    }
    return count;
}

static int
role_is_valid(const DBPartialOrderRole *role, uint32_t valid_roles_mask,
    size_t role_index)
{
    if (!direction_is_valid(role->direction) ||
        !kind_is_valid(role->kind) || role->required_succeeded > 1U ||
        !endpoint_matches_direction(role->endpoint, role->direction) ||
        role->allowed_length_count == 0U ||
        role->allowed_length_count > DB_PARTIAL_ORDER_MAX_ALLOWED_LENGTHS ||
        (role->predecessor_mask & ~valid_roles_mask) != 0U ||
        (role->predecessor_mask & (UINT32_C(1) << role_index)) != 0U) {
        return 0;
    }
    size_t previous_length = 0;
    for (size_t index = 0;
         index < DB_PARTIAL_ORDER_MAX_ALLOWED_LENGTHS; ++index) {
        size_t length = role->allowed_lengths[index];
        if (index < role->allowed_length_count) {
            if (length == 0U || length > DB_PROTOCOL_MAX_TRANSFER_LENGTH ||
                (index > 0U && length <= previous_length) ||
                (role->direction == DB_PROTOCOL_DIRECTION_IN &&
                    length < DB_PROTOCOL_IN_PREFIX_SIZE)) {
                return 0;
            }
            previous_length = length;
        } else if (length != 0U) {
            return 0;
        }
    }
    return 1;
}

static int
roles_form_dag(const DBPartialOrderRole *roles, size_t role_count,
    uint32_t all_roles_mask)
{
    uint32_t completed = 0;
    while (completed != all_roles_mask) {
        uint32_t previous = completed;
        for (size_t index = 0; index < role_count; ++index) {
            uint32_t bit = UINT32_C(1) << index;
            if ((completed & bit) == 0U &&
                (roles[index].predecessor_mask & ~completed) == 0U) {
                completed |= bit;
            }
        }
        if (completed == previous) {
            return 0;
        }
    }
    return 1;
}

static int
model_is_valid(const DBPartialOrderRole *roles, size_t role_count,
    uint32_t all_roles_mask)
{
    if (roles == NULL || role_count == 0U ||
        role_count > DB_PARTIAL_ORDER_MAX_ROLES ||
        all_roles_mask != roles_mask(role_count)) {
        return 0;
    }
    for (size_t index = 0; index < role_count; ++index) {
        if (!role_is_valid(&roles[index], all_roles_mask, index)) {
            return 0;
        }
    }
    return roles_form_dag(roles, role_count, all_roles_mask);
}

static int
state_is_valid(DBPartialOrderState state)
{
    switch (state) {
    case DB_PARTIAL_ORDER_STATE_WAITING:
    case DB_PARTIAL_ORDER_STATE_IN_PROGRESS:
    case DB_PARTIAL_ORDER_STATE_COMPLETE:
    case DB_PARTIAL_ORDER_STATE_FAILED:
        return 1;
    }
    return 0;
}

static int
candidate_is_closed(const DBPartialOrderMatcher *matcher, uint32_t mask)
{
    for (size_t role_index = 0; role_index < matcher->role_count;
         ++role_index) {
        uint32_t bit = UINT32_C(1) << role_index;
        if ((mask & bit) != 0U &&
            (matcher->roles[role_index].predecessor_mask & mask) !=
                matcher->roles[role_index].predecessor_mask) {
            return 0;
        }
    }
    return 1;
}

static int
matcher_is_consistent(const DBPartialOrderMatcher *matcher)
{
    if (!state_is_valid(matcher->state) ||
        matcher->state == DB_PARTIAL_ORDER_STATE_FAILED ||
        !model_is_valid(matcher->roles, matcher->role_count,
            matcher->all_roles_mask) ||
        matcher->consumed_transfer_count > matcher->role_count ||
        matcher->candidate_count == 0U ||
        matcher->candidate_count > DB_PARTIAL_ORDER_MAX_CANDIDATES) {
        return 0;
    }
    for (size_t index = 0; index < DB_PARTIAL_ORDER_MAX_CANDIDATES;
         ++index) {
        uint32_t mask = matcher->candidate_masks[index];
        if (index < matcher->candidate_count) {
            if ((mask & ~matcher->all_roles_mask) != 0U ||
                mask_population(mask) != matcher->consumed_transfer_count ||
                !candidate_is_closed(matcher, mask) ||
                (index > 0U &&
                    matcher->candidate_masks[index - 1U] >= mask)) {
                return 0;
            }
        } else if (mask != 0U) {
            return 0;
        }
    }
    switch (matcher->state) {
    case DB_PARTIAL_ORDER_STATE_WAITING:
        return matcher->consumed_transfer_count == 0U &&
            matcher->candidate_count == 1U &&
            matcher->candidate_masks[0] == 0U;
    case DB_PARTIAL_ORDER_STATE_IN_PROGRESS:
        return matcher->consumed_transfer_count > 0U &&
            matcher->consumed_transfer_count < matcher->role_count;
    case DB_PARTIAL_ORDER_STATE_COMPLETE:
        return matcher->consumed_transfer_count == matcher->role_count &&
            matcher->candidate_count == 1U &&
            matcher->candidate_masks[0] == matcher->all_roles_mask;
    case DB_PARTIAL_ORDER_STATE_FAILED:
        break;
    }
    return 0;
}

static int
role_accepts_transfer(const DBPartialOrderRole *role,
    const DBProtocolTransferMetadata *transfer)
{
    if (role->direction != transfer->direction ||
        role->endpoint != transfer->endpoint || role->kind != transfer->kind ||
        role->required_succeeded != transfer->succeeded) {
        return 0;
    }
    for (size_t index = 0; index < role->allowed_length_count; ++index) {
        if (role->allowed_lengths[index] == transfer->length) {
            return 1;
        }
    }
    return 0;
}

static int
insert_candidate(uint32_t *candidates, size_t *candidate_count,
    uint32_t candidate)
{
    size_t index = 0;
    while (index < *candidate_count && candidates[index] < candidate) {
        ++index;
    }
    if (index < *candidate_count && candidates[index] == candidate) {
        return 1;
    }
    if (*candidate_count == DB_PARTIAL_ORDER_MAX_CANDIDATES) {
        return 0;
    }
    for (size_t move = *candidate_count; move > index; --move) {
        candidates[move] = candidates[move - 1U];
    }
    candidates[index] = candidate;
    ++*candidate_count;
    return 1;
}

static DBPartialOrderResult
fail_matcher(DBPartialOrderMatcher *matcher, DBPartialOrderResult result)
{
    matcher->state = DB_PARTIAL_ORDER_STATE_FAILED;
    return result;
}

DBPartialOrderResult
db_partial_order_matcher_initialize(DBPartialOrderMatcher *matcher,
    const DBPartialOrderRole *roles, size_t role_count)
{
    if (matcher == NULL) {
        return DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT;
    }
    memset(matcher, 0, sizeof(*matcher));
    matcher->state = DB_PARTIAL_ORDER_STATE_FAILED;
    if (roles == NULL) {
        return DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT;
    }
    if (role_count == 0U || role_count > DB_PARTIAL_ORDER_MAX_ROLES) {
        return DB_PARTIAL_ORDER_RESULT_INVALID_MODEL;
    }
    matcher->role_count = role_count;
    matcher->all_roles_mask = roles_mask(role_count);
    for (size_t index = 0; index < role_count; ++index) {
        matcher->roles[index] = roles[index];
    }
    if (!model_is_valid(matcher->roles, matcher->role_count,
            matcher->all_roles_mask)) {
        return DB_PARTIAL_ORDER_RESULT_INVALID_MODEL;
    }
    matcher->candidate_count = 1U;
    matcher->candidate_masks[0] = 0U;
    matcher->state = DB_PARTIAL_ORDER_STATE_WAITING;
    return DB_PARTIAL_ORDER_RESULT_OK;
}

DBPartialOrderResult
db_partial_order_matcher_accept(DBPartialOrderMatcher *matcher,
    const DBProtocolTransferMetadata *transfer)
{
    if (matcher == NULL) {
        return DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT;
    }
    if (matcher->state == DB_PARTIAL_ORDER_STATE_FAILED) {
        return DB_PARTIAL_ORDER_RESULT_FAILED;
    }
    if (!matcher_is_consistent(matcher)) {
        return fail_matcher(matcher, DB_PARTIAL_ORDER_RESULT_CORRUPT_STATE);
    }
    if (matcher->state == DB_PARTIAL_ORDER_STATE_COMPLETE) {
        return DB_PARTIAL_ORDER_RESULT_ALREADY_COMPLETE;
    }
    if (transfer == NULL) {
        return fail_matcher(matcher,
            DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT);
    }
    if (!db_protocol_transfer_metadata_is_valid(transfer)) {
        return fail_matcher(matcher,
            DB_PARTIAL_ORDER_RESULT_INVALID_TRANSFER);
    }

    uint32_t next_candidates[DB_PARTIAL_ORDER_MAX_CANDIDATES] = {0};
    size_t next_candidate_count = 0;
    for (size_t candidate_index = 0;
         candidate_index < matcher->candidate_count; ++candidate_index) {
        uint32_t completed = matcher->candidate_masks[candidate_index];
        for (size_t role_index = 0; role_index < matcher->role_count;
             ++role_index) {
            uint32_t bit = UINT32_C(1) << role_index;
            const DBPartialOrderRole *role = &matcher->roles[role_index];
            if ((completed & bit) == 0U &&
                (role->predecessor_mask & completed) ==
                    role->predecessor_mask &&
                role_accepts_transfer(role, transfer)) {
                if (!insert_candidate(next_candidates,
                        &next_candidate_count, completed | bit)) {
                    return fail_matcher(matcher,
                        DB_PARTIAL_ORDER_RESULT_CANDIDATE_LIMIT);
                }
            }
        }
    }
    if (next_candidate_count == 0U) {
        return fail_matcher(matcher,
            DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER);
    }

    memset(matcher->candidate_masks, 0, sizeof(matcher->candidate_masks));
    memcpy(matcher->candidate_masks, next_candidates,
        next_candidate_count * sizeof(next_candidates[0]));
    matcher->candidate_count = next_candidate_count;
    ++matcher->consumed_transfer_count;
    if (matcher->consumed_transfer_count == matcher->role_count) {
        matcher->state = DB_PARTIAL_ORDER_STATE_COMPLETE;
        return DB_PARTIAL_ORDER_RESULT_COMPLETE;
    }
    matcher->state = DB_PARTIAL_ORDER_STATE_IN_PROGRESS;
    return DB_PARTIAL_ORDER_RESULT_OK;
}

DBPartialOrderResult
db_partial_order_matcher_finish(DBPartialOrderMatcher *matcher)
{
    if (matcher == NULL) {
        return DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT;
    }
    if (matcher->state == DB_PARTIAL_ORDER_STATE_FAILED) {
        return DB_PARTIAL_ORDER_RESULT_FAILED;
    }
    if (!matcher_is_consistent(matcher)) {
        return fail_matcher(matcher, DB_PARTIAL_ORDER_RESULT_CORRUPT_STATE);
    }
    if (matcher->state == DB_PARTIAL_ORDER_STATE_COMPLETE) {
        return DB_PARTIAL_ORDER_RESULT_COMPLETE;
    }
    return fail_matcher(matcher, DB_PARTIAL_ORDER_RESULT_INCOMPLETE);
}

const char *
db_partial_order_state_name(DBPartialOrderState state)
{
    switch (state) {
    case DB_PARTIAL_ORDER_STATE_WAITING:
        return "waiting";
    case DB_PARTIAL_ORDER_STATE_IN_PROGRESS:
        return "in-progress";
    case DB_PARTIAL_ORDER_STATE_COMPLETE:
        return "complete";
    case DB_PARTIAL_ORDER_STATE_FAILED:
        return "failed";
    }
    return "invalid-state";
}

const char *
db_partial_order_result_name(DBPartialOrderResult result)
{
    switch (result) {
    case DB_PARTIAL_ORDER_RESULT_OK:
        return "ok";
    case DB_PARTIAL_ORDER_RESULT_COMPLETE:
        return "complete";
    case DB_PARTIAL_ORDER_RESULT_INCOMPLETE:
        return "incomplete";
    case DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT:
        return "invalid-argument";
    case DB_PARTIAL_ORDER_RESULT_INVALID_MODEL:
        return "invalid-model";
    case DB_PARTIAL_ORDER_RESULT_INVALID_TRANSFER:
        return "invalid-transfer";
    case DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER:
        return "unexpected-transfer";
    case DB_PARTIAL_ORDER_RESULT_CANDIDATE_LIMIT:
        return "candidate-limit";
    case DB_PARTIAL_ORDER_RESULT_CORRUPT_STATE:
        return "corrupt-state";
    case DB_PARTIAL_ORDER_RESULT_FAILED:
        return "failed";
    case DB_PARTIAL_ORDER_RESULT_ALREADY_COMPLETE:
        return "already-complete";
    }
    return "invalid-result";
}
