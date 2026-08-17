#ifndef DOCKBRIDGE_QUALIFIED_SEQUENCES_H
#define DOCKBRIDGE_QUALIFIED_SEQUENCES_H

#include "partial_order_matcher.h"

#include <stddef.h>

/*
 * These sequence names describe sanitized trace-shape correlations only.
 * They do not name protocol commands, assign field meaning, or authorize a
 * transfer to be constructed or sent.
 */
typedef enum {
    DB_QUALIFIED_SEQUENCE_INVALID = 0,
    DB_QUALIFIED_SEQUENCE_STARTUP_FIRST_BURST,
    DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX,
    DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A,
    DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B
} DBQualifiedSequenceKind;

typedef struct {
    DBQualifiedSequenceKind kind;
    const char *fact_id;
    const char *evidence_maturity;
    const DBPartialOrderRole *roles;
    size_t role_count;
} DBQualifiedSequence;

int db_qualified_sequence_get(DBQualifiedSequenceKind kind,
    DBQualifiedSequence *sequence);
const char *db_qualified_sequence_kind_name(DBQualifiedSequenceKind kind);

#endif
