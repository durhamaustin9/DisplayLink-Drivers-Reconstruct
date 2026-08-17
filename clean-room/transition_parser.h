#ifndef DOCKBRIDGE_TRANSITION_PARSER_H
#define DOCKBRIDGE_TRANSITION_PARSER_H

#include "partial_order_matcher.h"
#include "state_machine.h"

#include <stddef.h>
#include <stdint.h>

enum {
    DB_TRANSITION_MAX_SMALL_TRANSFER = 1024,
    DB_TRANSITION_HOTPLUG_REACTION_ROLE_COUNT = 15,
    DB_TRANSITION_HOTPLUG_PREFIX_ROLE_COUNT = 24,
    DB_TRANSITION_HOTUNPLUG_PROFILE_ROLE_COUNT = 29
};

typedef enum {
    DB_TRANSITION_KIND_INVALID = 0,
    DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX,
    DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE
} DBTransitionKind;

typedef enum {
    DB_TRANSITION_STATE_WAITING = 0,
    DB_TRANSITION_STATE_IN_PROGRESS,
    DB_TRANSITION_STATE_COMPLETE,
    DB_TRANSITION_STATE_FAILED
} DBTransitionState;

typedef enum {
    DB_TRANSITION_RESULT_OK = 0,
    DB_TRANSITION_RESULT_COMPLETE,
    DB_TRANSITION_RESULT_INCOMPLETE,
    DB_TRANSITION_RESULT_INVALID_ARGUMENT,
    DB_TRANSITION_RESULT_INVALID_KIND,
    DB_TRANSITION_RESULT_BINDING_REQUIRED,
    DB_TRANSITION_RESULT_BINDING_LOST,
    DB_TRANSITION_RESULT_INVALID_TRANSFER,
    DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER,
    DB_TRANSITION_RESULT_CORRUPT_STATE,
    DB_TRANSITION_RESULT_FAILED,
    DB_TRANSITION_RESULT_ALREADY_COMPLETE
} DBTransitionResult;

typedef enum {
    DB_TRANSITION_PROFILE_NONE = 0,
    DB_TRANSITION_PROFILE_HOTPLUG_PREFIX,
    DB_TRANSITION_PROFILE_HOTUNPLUG_A,
    DB_TRANSITION_PROFILE_HOTUNPLUG_B
} DBTransitionObservedProfile;

typedef enum {
    DB_TRANSITION_STAGE_WAITING = 0,
    DB_TRANSITION_STAGE_PREFIX_BEFORE_15,
    DB_TRANSITION_STAGE_FIRST_15_OBSERVED,
    DB_TRANSITION_STAGE_COMPLETE,
    DB_TRANSITION_STAGE_NOT_APPLICABLE
} DBTransitionStage;

/*
 * The parser owns no transport and emits no bytes. It is bound to the
 * generation and transport lifecycle epoch of an exact, topology-verified
 * fake machine and accepts only sanitized transfer-envelope metadata.
 */
typedef struct {
    DBTransitionKind kind;
    DBTransitionState state;
    const DBMachine *machine;
    uint64_t machine_generation;
    uint64_t transport_lifecycle_epoch;
    size_t accepted_count;
    uint8_t active_profile_mask;
    DBPartialOrderMatcher candidates[2];
} DBTransitionParser;

DBTransitionResult db_transition_parser_initialize(DBTransitionParser *parser,
    DBTransitionKind kind, const DBMachine *verified_fake_machine);
DBTransitionResult db_transition_parser_accept(DBTransitionParser *parser,
    const DBProtocolTransferMetadata *transfer);
DBTransitionResult db_transition_parser_finish(DBTransitionParser *parser);
DBTransitionObservedProfile db_transition_parser_observed_profile(
    const DBTransitionParser *parser);
DBTransitionStage db_transition_parser_stage(
    const DBTransitionParser *parser);
const char *db_transition_kind_name(DBTransitionKind kind);
const char *db_transition_state_name(DBTransitionState state);
const char *db_transition_result_name(DBTransitionResult result);
const char *db_transition_profile_name(DBTransitionObservedProfile profile);
const char *db_transition_stage_name(DBTransitionStage stage);

#endif
