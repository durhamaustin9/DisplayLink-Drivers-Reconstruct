#include "qualified_sequences.h"

#include <stdint.h>

enum {
    DB_ENDPOINT_OUT = 0x02,
    DB_ENDPOINT_IN = 0x84,
    DB_STARTUP_ROLE_COUNT = 15,
    DB_HOTPLUG_ROLE_COUNT = 24,
    DB_HOTUNPLUG_ROLE_COUNT = 29
};

#define DB_ROLE(direction_value, endpoint_value, length_value, predecessors) \
    {                                                                       \
        .direction = (direction_value),                                     \
        .endpoint = (endpoint_value),                                       \
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,                             \
        .required_succeeded = 1U,                                           \
        .allowed_length_count = 1U,                                         \
        .allowed_lengths = {(length_value), 0U, 0U, 0U},                    \
        .predecessor_mask = (predecessors)                                  \
    }

#define DB_OUT(length_value, predecessors) \
    DB_ROLE(DB_PROTOCOL_DIRECTION_OUT, DB_ENDPOINT_OUT, (length_value),     \
        (predecessors))
#define DB_IN(length_value, predecessors) \
    DB_ROLE(DB_PROTOCOL_DIRECTION_IN, DB_ENDPOINT_IN, (length_value),       \
        (predecessors))
#define DB_PREVIOUS(index) (UINT32_C(1) << ((index) - 1U))

/* FACT-STARTUP-ROLES-001 and FACT-STARTUP-ORDER-001. */
static const DBPartialOrderRole startup_first_burst[DB_STARTUP_ROLE_COUNT] = {
    DB_OUT(16, 0U),
    DB_OUT(32, DB_PREVIOUS(1)),
    DB_OUT(80, DB_PREVIOUS(2)),
    DB_IN(39, DB_PREVIOUS(3)),
    DB_OUT(48, DB_PREVIOUS(4)),
    DB_IN(38, DB_PREVIOUS(5)),
    DB_OUT(64, DB_PREVIOUS(6)),
    DB_IN(38, DB_PREVIOUS(7)),
    DB_OUT(64, DB_PREVIOUS(8)),
    /* Roles 9 and 10 may occur in either order after role 8. */
    DB_IN(38, UINT32_C(1) << 8U),
    DB_IN(549, UINT32_C(1) << 8U),
    DB_IN(31, (UINT32_C(1) << 9U) | (UINT32_C(1) << 10U)),
    DB_OUT(176, DB_PREVIOUS(12)),
    DB_IN(38, DB_PREVIOUS(13)),
    DB_IN(34, DB_PREVIOUS(14))
};

/* FACT-HOTPLUG-PREFIX-001: one provisional 24-role closed prefix. */
static const DBPartialOrderRole hotplug_prefix[DB_HOTPLUG_ROLE_COUNT] = {
    DB_IN(48, 0U),
    DB_OUT(64, DB_PREVIOUS(1)),
    DB_IN(128, DB_PREVIOUS(2)),
    DB_IN(48, DB_PREVIOUS(3)),
    DB_OUT(64, DB_PREVIOUS(4)),
    DB_IN(112, DB_PREVIOUS(5)),
    DB_OUT(64, DB_PREVIOUS(6)),
    DB_IN(64, DB_PREVIOUS(7)),
    DB_OUT(64, DB_PREVIOUS(8)),
    DB_IN(64, DB_PREVIOUS(9)),
    DB_IN(48, DB_PREVIOUS(10)),
    DB_OUT(64, DB_PREVIOUS(11)),
    DB_IN(128, DB_PREVIOUS(12)),
    DB_OUT(64, DB_PREVIOUS(13)),
    DB_IN(64, DB_PREVIOUS(14)),
    DB_IN(48, DB_PREVIOUS(15)),
    DB_OUT(64, DB_PREVIOUS(16)),
    DB_IN(112, DB_PREVIOUS(17)),
    DB_OUT(64, DB_PREVIOUS(18)),
    DB_IN(320, DB_PREVIOUS(19)),
    DB_IN(48, DB_PREVIOUS(20)),
    DB_OUT(64, DB_PREVIOUS(21)),
    DB_IN(64, DB_PREVIOUS(22)),
    DB_OUT(64, DB_PREVIOUS(23))
};

/*
 * FACT-HOTUNPLUG-PROFILES-001. These are two closed observed profiles. They
 * intentionally are not factored into independent wildcard choices because
 * no cross-profile combination has been observed.
 */
static const DBPartialOrderRole hotunplug_profile_a[
    DB_HOTUNPLUG_ROLE_COUNT] = {
    DB_IN(48, 0U),
    DB_OUT(64, DB_PREVIOUS(1)),
    DB_IN(112, DB_PREVIOUS(2)),
    DB_OUT(112, DB_PREVIOUS(3)),
    DB_IN(64, DB_PREVIOUS(4)),
    DB_IN(48, DB_PREVIOUS(5)),
    DB_OUT(64, DB_PREVIOUS(6)),
    DB_IN(64, DB_PREVIOUS(7)),
    DB_OUT(64, DB_PREVIOUS(8)),
    DB_IN(112, DB_PREVIOUS(9)),
    DB_IN(48, DB_PREVIOUS(10)),
    DB_OUT(96, DB_PREVIOUS(11)),
    DB_IN(128, DB_PREVIOUS(12)),
    DB_IN(48, DB_PREVIOUS(13)),
    DB_OUT(64, DB_PREVIOUS(14)),
    DB_IN(560, DB_PREVIOUS(15)),
    DB_IN(48, DB_PREVIOUS(16)),
    DB_OUT(64, DB_PREVIOUS(17)),
    DB_IN(768, DB_PREVIOUS(18)),
    DB_IN(48, DB_PREVIOUS(19)),
    DB_OUT(64, DB_PREVIOUS(20)),
    DB_IN(384, DB_PREVIOUS(21)),
    DB_IN(48, DB_PREVIOUS(22)),
    DB_OUT(64, DB_PREVIOUS(23)),
    DB_IN(672, DB_PREVIOUS(24)),
    DB_OUT(64, DB_PREVIOUS(25)),
    DB_IN(64, DB_PREVIOUS(26)),
    DB_OUT(64, DB_PREVIOUS(27)),
    DB_IN(64, DB_PREVIOUS(28))
};

static const DBPartialOrderRole hotunplug_profile_b[
    DB_HOTUNPLUG_ROLE_COUNT] = {
    DB_IN(48, 0U),
    DB_OUT(64, DB_PREVIOUS(1)),
    DB_IN(112, DB_PREVIOUS(2)),
    DB_OUT(112, DB_PREVIOUS(3)),
    DB_IN(64, DB_PREVIOUS(4)),
    DB_OUT(64, DB_PREVIOUS(5)),
    DB_IN(48, DB_PREVIOUS(6)),
    DB_IN(64, DB_PREVIOUS(7)),
    DB_OUT(64, DB_PREVIOUS(8)),
    DB_IN(112, DB_PREVIOUS(9)),
    DB_IN(48, DB_PREVIOUS(10)),
    DB_OUT(96, DB_PREVIOUS(11)),
    DB_IN(128, DB_PREVIOUS(12)),
    DB_IN(48, DB_PREVIOUS(13)),
    DB_OUT(64, DB_PREVIOUS(14)),
    DB_IN(496, DB_PREVIOUS(15)),
    DB_IN(48, DB_PREVIOUS(16)),
    DB_OUT(64, DB_PREVIOUS(17)),
    DB_IN(784, DB_PREVIOUS(18)),
    DB_IN(48, DB_PREVIOUS(19)),
    DB_OUT(64, DB_PREVIOUS(20)),
    DB_IN(752, DB_PREVIOUS(21)),
    DB_IN(48, DB_PREVIOUS(22)),
    DB_OUT(64, DB_PREVIOUS(23)),
    DB_IN(368, DB_PREVIOUS(24)),
    DB_OUT(64, DB_PREVIOUS(25)),
    DB_IN(64, DB_PREVIOUS(26)),
    DB_OUT(64, DB_PREVIOUS(27)),
    DB_IN(64, DB_PREVIOUS(28))
};

int
db_qualified_sequence_get(DBQualifiedSequenceKind kind,
    DBQualifiedSequence *sequence)
{
    if (sequence == NULL) {
        return 0;
    }
    switch (kind) {
    case DB_QUALIFIED_SEQUENCE_STARTUP_FIRST_BURST:
        *sequence = (DBQualifiedSequence) {
            kind,
            "FACT-STARTUP-ROLES-001",
            "E3",
            startup_first_burst,
            DB_STARTUP_ROLE_COUNT
        };
        return 1;
    case DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX:
        *sequence = (DBQualifiedSequence) {
            kind,
            "FACT-HOTPLUG-PREFIX-001",
            "E2",
            hotplug_prefix,
            DB_HOTPLUG_ROLE_COUNT
        };
        return 1;
    case DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A:
        *sequence = (DBQualifiedSequence) {
            kind,
            "FACT-HOTUNPLUG-PROFILES-001",
            "E2",
            hotunplug_profile_a,
            DB_HOTUNPLUG_ROLE_COUNT
        };
        return 1;
    case DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B:
        *sequence = (DBQualifiedSequence) {
            kind,
            "FACT-HOTUNPLUG-PROFILES-001",
            "E2",
            hotunplug_profile_b,
            DB_HOTUNPLUG_ROLE_COUNT
        };
        return 1;
    case DB_QUALIFIED_SEQUENCE_INVALID:
        break;
    }
    *sequence = (DBQualifiedSequence) {0};
    return 0;
}

const char *
db_qualified_sequence_kind_name(DBQualifiedSequenceKind kind)
{
    switch (kind) {
    case DB_QUALIFIED_SEQUENCE_STARTUP_FIRST_BURST:
        return "startup-first-burst";
    case DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX:
        return "hotplug-correlated-prefix";
    case DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A:
        return "hotunplug-correlated-profile-a";
    case DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B:
        return "hotunplug-correlated-profile-b";
    case DB_QUALIFIED_SEQUENCE_INVALID:
        return "invalid";
    }
    return "invalid";
}

#undef DB_PREVIOUS
#undef DB_IN
#undef DB_OUT
#undef DB_ROLE
