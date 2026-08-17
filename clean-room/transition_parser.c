#include "transition_parser.h"

#include "qualified_sequences.h"

#include <string.h>

enum {
    DB_PROFILE_FIRST = 1U,
    DB_PROFILE_SECOND = 2U
};

static int
kind_is_valid(DBTransitionKind kind)
{
    return kind == DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX ||
        kind == DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE;
}

static size_t
expected_count(DBTransitionKind kind)
{
    return kind == DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX ?
        DB_TRANSITION_HOTPLUG_PREFIX_ROLE_COUNT :
        DB_TRANSITION_HOTUNPLUG_PROFILE_ROLE_COUNT;
}

static int
state_is_valid(DBTransitionState state)
{
    switch (state) {
    case DB_TRANSITION_STATE_WAITING:
    case DB_TRANSITION_STATE_IN_PROGRESS:
    case DB_TRANSITION_STATE_COMPLETE:
    case DB_TRANSITION_STATE_FAILED:
        return 1;
    }
    return 0;
}

static int
candidate_matches_progress(const DBPartialOrderMatcher *candidate,
    size_t accepted_count, size_t total_count)
{
    if (candidate->consumed_transfer_count != accepted_count) {
        return 0;
    }
    if (accepted_count == 0U) {
        return candidate->state == DB_PARTIAL_ORDER_STATE_WAITING;
    }
    if (accepted_count == total_count) {
        return candidate->state == DB_PARTIAL_ORDER_STATE_COMPLETE;
    }
    return candidate->state == DB_PARTIAL_ORDER_STATE_IN_PROGRESS;
}

static int
parser_is_consistent(const DBTransitionParser *parser)
{
    if (parser == NULL || !kind_is_valid(parser->kind) ||
        !state_is_valid(parser->state) ||
        parser->state == DB_TRANSITION_STATE_FAILED ||
        parser->machine == NULL || parser->machine_generation == 0U ||
        parser->transport_lifecycle_epoch == 0U ||
        parser->accepted_count > expected_count(parser->kind)) {
        return 0;
    }

    uint8_t allowed_mask = parser->kind ==
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX ?
        DB_PROFILE_FIRST : DB_PROFILE_FIRST | DB_PROFILE_SECOND;
    if (parser->active_profile_mask == 0U ||
        (parser->active_profile_mask & ~allowed_mask) != 0U ||
        (parser->kind == DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX &&
            parser->active_profile_mask != DB_PROFILE_FIRST)) {
        return 0;
    }
    for (size_t index = 0; index < 2U; ++index) {
        uint8_t bit = (uint8_t)(1U << index);
        if ((parser->active_profile_mask & bit) != 0U &&
            !candidate_matches_progress(&parser->candidates[index],
                parser->accepted_count, expected_count(parser->kind))) {
            return 0;
        }
    }

    switch (parser->state) {
    case DB_TRANSITION_STATE_WAITING:
        return parser->accepted_count == 0U;
    case DB_TRANSITION_STATE_IN_PROGRESS:
        return parser->accepted_count > 0U &&
            parser->accepted_count < expected_count(parser->kind);
    case DB_TRANSITION_STATE_COMPLETE:
        return parser->accepted_count == expected_count(parser->kind);
    case DB_TRANSITION_STATE_FAILED:
        break;
    }
    return 0;
}

static int
machine_binding_is_live(const DBTransitionParser *parser)
{
    return parser->machine != NULL &&
        parser->machine->generation == parser->machine_generation &&
        parser->machine->transport_lifecycle_epoch ==
            parser->transport_lifecycle_epoch &&
        db_transport_lifecycle_epoch(parser->machine->transport) ==
            parser->transport_lifecycle_epoch &&
        db_machine_is_exact_verified(parser->machine);
}

static DBTransitionResult
fail_parser(DBTransitionParser *parser, DBTransitionResult result)
{
    parser->state = DB_TRANSITION_STATE_FAILED;
    return result;
}

static int
active_matchers_validate_for_finish(const DBTransitionParser *parser)
{
    for (size_t index = 0; index < 2U; ++index) {
        uint8_t bit = (uint8_t)(1U << index);
        if ((parser->active_profile_mask & bit) == 0U) {
            continue;
        }
        DBPartialOrderMatcher copy = parser->candidates[index];
        DBPartialOrderResult result = db_partial_order_matcher_finish(&copy);
        if (parser->state == DB_TRANSITION_STATE_COMPLETE) {
            if (result != DB_PARTIAL_ORDER_RESULT_COMPLETE) {
                return 0;
            }
        } else if (result != DB_PARTIAL_ORDER_RESULT_INCOMPLETE) {
            return 0;
        }
    }
    return 1;
}

static int
parser_is_live_and_consistent(const DBTransitionParser *parser)
{
    return parser != NULL && parser->state != DB_TRANSITION_STATE_FAILED &&
        parser_is_consistent(parser) &&
        active_matchers_validate_for_finish(parser) &&
        machine_binding_is_live(parser);
}

static int
transfer_is_qualified(const DBProtocolTransferMetadata *transfer)
{
    if (!db_protocol_transfer_metadata_is_valid(transfer) ||
        transfer->kind != DB_PROTOCOL_TRANSFER_KIND_BULK ||
        transfer->succeeded != 1U ||
        transfer->length > DB_TRANSITION_MAX_SMALL_TRANSFER) {
        return 0;
    }
    if (transfer->direction == DB_PROTOCOL_DIRECTION_OUT) {
        return transfer->endpoint == DB_MACHINE_ENDPOINT_OUT &&
            transfer->length % 16U == 0U;
    }
    return transfer->direction == DB_PROTOCOL_DIRECTION_IN &&
        transfer->endpoint == DB_MACHINE_ENDPOINT_IN;
}

static int
initialize_candidate(DBPartialOrderMatcher *candidate,
    DBQualifiedSequenceKind kind)
{
    DBQualifiedSequence sequence = {0};
    return db_qualified_sequence_get(kind, &sequence) &&
        db_partial_order_matcher_initialize(candidate, sequence.roles,
            sequence.role_count) == DB_PARTIAL_ORDER_RESULT_OK;
}

DBTransitionResult
db_transition_parser_initialize(DBTransitionParser *parser,
    DBTransitionKind kind, const DBMachine *verified_fake_machine)
{
    if (parser == NULL) {
        return DB_TRANSITION_RESULT_INVALID_ARGUMENT;
    }
    memset(parser, 0, sizeof(*parser));
    parser->state = DB_TRANSITION_STATE_FAILED;
    if (!kind_is_valid(kind)) {
        return DB_TRANSITION_RESULT_INVALID_KIND;
    }
    if (!db_machine_is_exact_verified(verified_fake_machine) ||
        verified_fake_machine->generation == 0U) {
        return DB_TRANSITION_RESULT_BINDING_REQUIRED;
    }

    parser->kind = kind;
    parser->machine = verified_fake_machine;
    parser->machine_generation = verified_fake_machine->generation;
    parser->transport_lifecycle_epoch =
        verified_fake_machine->transport_lifecycle_epoch;
    if (kind == DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX) {
        if (!initialize_candidate(&parser->candidates[0],
                DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX)) {
            return DB_TRANSITION_RESULT_CORRUPT_STATE;
        }
        parser->active_profile_mask = DB_PROFILE_FIRST;
    } else {
        if (!initialize_candidate(&parser->candidates[0],
                DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A) ||
            !initialize_candidate(&parser->candidates[1],
                DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B)) {
            return DB_TRANSITION_RESULT_CORRUPT_STATE;
        }
        parser->active_profile_mask = DB_PROFILE_FIRST | DB_PROFILE_SECOND;
    }
    parser->state = DB_TRANSITION_STATE_WAITING;
    return DB_TRANSITION_RESULT_OK;
}

DBTransitionResult
db_transition_parser_accept(DBTransitionParser *parser,
    const DBProtocolTransferMetadata *transfer)
{
    if (parser == NULL) {
        return DB_TRANSITION_RESULT_INVALID_ARGUMENT;
    }
    if (parser->state == DB_TRANSITION_STATE_FAILED) {
        return DB_TRANSITION_RESULT_FAILED;
    }
    if (!parser_is_consistent(parser)) {
        return fail_parser(parser, DB_TRANSITION_RESULT_CORRUPT_STATE);
    }
    if (!active_matchers_validate_for_finish(parser)) {
        return fail_parser(parser, DB_TRANSITION_RESULT_CORRUPT_STATE);
    }
    if (!machine_binding_is_live(parser)) {
        return fail_parser(parser, DB_TRANSITION_RESULT_BINDING_LOST);
    }
    if (parser->state == DB_TRANSITION_STATE_COMPLETE) {
        return DB_TRANSITION_RESULT_ALREADY_COMPLETE;
    }
    if (transfer == NULL) {
        return fail_parser(parser, DB_TRANSITION_RESULT_INVALID_ARGUMENT);
    }
    if (!transfer_is_qualified(transfer)) {
        return fail_parser(parser, DB_TRANSITION_RESULT_INVALID_TRANSFER);
    }

    uint8_t retained_mask = 0U;
    int any_complete = 0;
    for (size_t index = 0; index < 2U; ++index) {
        uint8_t bit = (uint8_t)(1U << index);
        if ((parser->active_profile_mask & bit) == 0U) {
            continue;
        }
        DBPartialOrderResult result = db_partial_order_matcher_accept(
            &parser->candidates[index], transfer);
        if (result == DB_PARTIAL_ORDER_RESULT_OK ||
            result == DB_PARTIAL_ORDER_RESULT_COMPLETE) {
            retained_mask |= bit;
            any_complete |= result == DB_PARTIAL_ORDER_RESULT_COMPLETE;
        } else if (result != DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER) {
            return fail_parser(parser, DB_TRANSITION_RESULT_CORRUPT_STATE);
        }
    }
    if (retained_mask == 0U) {
        return fail_parser(parser,
            DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER);
    }

    parser->active_profile_mask = retained_mask;
    ++parser->accepted_count;
    if (parser->accepted_count == expected_count(parser->kind)) {
        if (!any_complete) {
            return fail_parser(parser, DB_TRANSITION_RESULT_CORRUPT_STATE);
        }
        parser->state = DB_TRANSITION_STATE_COMPLETE;
        return DB_TRANSITION_RESULT_COMPLETE;
    }
    if (any_complete) {
        return fail_parser(parser, DB_TRANSITION_RESULT_CORRUPT_STATE);
    }
    parser->state = DB_TRANSITION_STATE_IN_PROGRESS;
    return DB_TRANSITION_RESULT_OK;
}

DBTransitionResult
db_transition_parser_finish(DBTransitionParser *parser)
{
    if (parser == NULL) {
        return DB_TRANSITION_RESULT_INVALID_ARGUMENT;
    }
    if (parser->state == DB_TRANSITION_STATE_FAILED) {
        return DB_TRANSITION_RESULT_FAILED;
    }
    if (!parser_is_consistent(parser)) {
        return fail_parser(parser, DB_TRANSITION_RESULT_CORRUPT_STATE);
    }
    if (!active_matchers_validate_for_finish(parser)) {
        return fail_parser(parser, DB_TRANSITION_RESULT_CORRUPT_STATE);
    }
    if (!machine_binding_is_live(parser)) {
        return fail_parser(parser, DB_TRANSITION_RESULT_BINDING_LOST);
    }
    if (parser->state == DB_TRANSITION_STATE_COMPLETE) {
        return DB_TRANSITION_RESULT_COMPLETE;
    }
    return fail_parser(parser, DB_TRANSITION_RESULT_INCOMPLETE);
}

DBTransitionObservedProfile
db_transition_parser_observed_profile(const DBTransitionParser *parser)
{
    if (!parser_is_live_and_consistent(parser) ||
        parser->state != DB_TRANSITION_STATE_COMPLETE) {
        return DB_TRANSITION_PROFILE_NONE;
    }
    if (parser->kind == DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX) {
        return DB_TRANSITION_PROFILE_HOTPLUG_PREFIX;
    }
    if (parser->active_profile_mask == DB_PROFILE_FIRST) {
        return DB_TRANSITION_PROFILE_HOTUNPLUG_A;
    }
    if (parser->active_profile_mask == DB_PROFILE_SECOND) {
        return DB_TRANSITION_PROFILE_HOTUNPLUG_B;
    }
    return DB_TRANSITION_PROFILE_NONE;
}

DBTransitionStage
db_transition_parser_stage(const DBTransitionParser *parser)
{
    if (!parser_is_live_and_consistent(parser) ||
        parser->kind != DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX) {
        return DB_TRANSITION_STAGE_NOT_APPLICABLE;
    }
    if (parser->state == DB_TRANSITION_STATE_COMPLETE) {
        return DB_TRANSITION_STAGE_COMPLETE;
    }
    if (parser->accepted_count == 0U) {
        return DB_TRANSITION_STAGE_WAITING;
    }
    return parser->accepted_count >=
        DB_TRANSITION_HOTPLUG_REACTION_ROLE_COUNT ?
        DB_TRANSITION_STAGE_FIRST_15_OBSERVED :
        DB_TRANSITION_STAGE_PREFIX_BEFORE_15;
}

const char *
db_transition_kind_name(DBTransitionKind kind)
{
    switch (kind) {
    case DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX:
        return "hotplug-correlated-prefix";
    case DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE:
        return "hotunplug-correlated-profile";
    case DB_TRANSITION_KIND_INVALID:
        return "invalid";
    }
    return "invalid";
}

const char *
db_transition_state_name(DBTransitionState state)
{
    switch (state) {
    case DB_TRANSITION_STATE_WAITING:
        return "waiting";
    case DB_TRANSITION_STATE_IN_PROGRESS:
        return "in-progress";
    case DB_TRANSITION_STATE_COMPLETE:
        return "complete";
    case DB_TRANSITION_STATE_FAILED:
        return "failed";
    }
    return "invalid-state";
}

const char *
db_transition_result_name(DBTransitionResult result)
{
    switch (result) {
    case DB_TRANSITION_RESULT_OK:
        return "ok";
    case DB_TRANSITION_RESULT_COMPLETE:
        return "complete";
    case DB_TRANSITION_RESULT_INCOMPLETE:
        return "incomplete";
    case DB_TRANSITION_RESULT_INVALID_ARGUMENT:
        return "invalid-argument";
    case DB_TRANSITION_RESULT_INVALID_KIND:
        return "invalid-kind";
    case DB_TRANSITION_RESULT_BINDING_REQUIRED:
        return "binding-required";
    case DB_TRANSITION_RESULT_BINDING_LOST:
        return "binding-lost";
    case DB_TRANSITION_RESULT_INVALID_TRANSFER:
        return "invalid-transfer";
    case DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER:
        return "unexpected-transfer";
    case DB_TRANSITION_RESULT_CORRUPT_STATE:
        return "corrupt-state";
    case DB_TRANSITION_RESULT_FAILED:
        return "failed";
    case DB_TRANSITION_RESULT_ALREADY_COMPLETE:
        return "already-complete";
    }
    return "invalid-result";
}

const char *
db_transition_profile_name(DBTransitionObservedProfile profile)
{
    switch (profile) {
    case DB_TRANSITION_PROFILE_NONE:
        return "none";
    case DB_TRANSITION_PROFILE_HOTPLUG_PREFIX:
        return "hotplug-correlated-prefix";
    case DB_TRANSITION_PROFILE_HOTUNPLUG_A:
        return "hotunplug-correlated-profile-a";
    case DB_TRANSITION_PROFILE_HOTUNPLUG_B:
        return "hotunplug-correlated-profile-b";
    }
    return "invalid-profile";
}

const char *
db_transition_stage_name(DBTransitionStage stage)
{
    switch (stage) {
    case DB_TRANSITION_STAGE_WAITING:
        return "waiting";
    case DB_TRANSITION_STAGE_PREFIX_BEFORE_15:
        return "prefix-before-15";
    case DB_TRANSITION_STAGE_FIRST_15_OBSERVED:
        return "first-15-observed";
    case DB_TRANSITION_STAGE_COMPLETE:
        return "complete";
    case DB_TRANSITION_STAGE_NOT_APPLICABLE:
        return "not-applicable";
    }
    return "invalid-stage";
}
