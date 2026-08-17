#include "qualified_sequences.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    DBProtocolDirection direction;
    size_t length;
} ExpectedTransfer;

#define EOUT(length_value) {DB_PROTOCOL_DIRECTION_OUT, (length_value)}
#define EIN(length_value) {DB_PROTOCOL_DIRECTION_IN, (length_value)}

static const ExpectedTransfer expected_startup[] = {
    EOUT(16), EOUT(32), EOUT(80), EIN(39), EOUT(48), EIN(38),
    EOUT(64), EIN(38), EOUT(64), EIN(38), EIN(549), EIN(31),
    EOUT(176), EIN(38), EIN(34)
};

static const ExpectedTransfer expected_hotplug[] = {
    EIN(48), EOUT(64), EIN(128), EIN(48), EOUT(64), EIN(112),
    EOUT(64), EIN(64), EOUT(64), EIN(64), EIN(48), EOUT(64),
    EIN(128), EOUT(64), EIN(64), EIN(48), EOUT(64), EIN(112),
    EOUT(64), EIN(320), EIN(48), EOUT(64), EIN(64), EOUT(64)
};

static const ExpectedTransfer expected_hotunplug_a[] = {
    EIN(48), EOUT(64), EIN(112), EOUT(112), EIN(64), EIN(48),
    EOUT(64), EIN(64), EOUT(64), EIN(112), EIN(48), EOUT(96),
    EIN(128), EIN(48), EOUT(64), EIN(560), EIN(48), EOUT(64),
    EIN(768), EIN(48), EOUT(64), EIN(384), EIN(48), EOUT(64),
    EIN(672), EOUT(64), EIN(64), EOUT(64), EIN(64)
};

static const ExpectedTransfer expected_hotunplug_b[] = {
    EIN(48), EOUT(64), EIN(112), EOUT(112), EIN(64), EOUT(64),
    EIN(48), EIN(64), EOUT(64), EIN(112), EIN(48), EOUT(96),
    EIN(128), EIN(48), EOUT(64), EIN(496), EIN(48), EOUT(64),
    EIN(784), EIN(48), EOUT(64), EIN(752), EIN(48), EOUT(64),
    EIN(368), EOUT(64), EIN(64), EOUT(64), EIN(64)
};

#undef EIN
#undef EOUT

static void
assert_pinned_sequence(DBQualifiedSequenceKind kind,
    const ExpectedTransfer *expected, size_t expected_count)
{
    DBQualifiedSequence sequence = {0};
    assert(db_qualified_sequence_get(kind, &sequence));
    assert(sequence.role_count == expected_count);
    for (size_t index = 0; index < expected_count; ++index) {
        const DBPartialOrderRole *role = &sequence.roles[index];
        assert(role->direction == expected[index].direction);
        assert(role->endpoint == (role->direction ==
            DB_PROTOCOL_DIRECTION_OUT ? 0x02U : 0x84U));
        assert(role->kind == DB_PROTOCOL_TRANSFER_KIND_BULK);
        assert(role->required_succeeded == 1U);
        assert(role->allowed_length_count == 1U);
        assert(role->allowed_lengths[0] == expected[index].length);
        assert(role->allowed_lengths[1] == 0U);
        assert(role->allowed_lengths[2] == 0U);
        assert(role->allowed_lengths[3] == 0U);
        uint32_t expected_predecessors = index == 0U ? 0U :
            UINT32_C(1) << (index - 1U);
        if (kind == DB_QUALIFIED_SEQUENCE_STARTUP_FIRST_BURST) {
            if (index == 9U || index == 10U) {
                expected_predecessors = UINT32_C(1) << 8U;
            } else if (index == 11U) {
                expected_predecessors =
                    (UINT32_C(1) << 9U) | (UINT32_C(1) << 10U);
            }
        }
        assert(role->predecessor_mask == expected_predecessors);
    }
}

static DBProtocolTransferMetadata
metadata_for(const DBPartialOrderRole *role)
{
    DBProtocolTransferMetadata metadata = {
        .direction = role->direction,
        .endpoint = role->endpoint,
        .kind = role->kind,
        .succeeded = role->required_succeeded,
        .length = role->allowed_lengths[0]
    };
    if (metadata.direction == DB_PROTOCOL_DIRECTION_IN) {
        size_t body_length = metadata.length - DB_PROTOCOL_IN_PREFIX_SIZE;
        metadata.in_prefix[2] = (uint8_t)(body_length & 0xffU);
        metadata.in_prefix[3] = (uint8_t)((body_length >> 8U) & 0xffU);
    }
    return metadata;
}

static void
run_chain(DBQualifiedSequenceKind kind, size_t expected_count,
    const char *expected_fact, const char *expected_maturity)
{
    DBQualifiedSequence sequence = {0};
    assert(db_qualified_sequence_get(kind, &sequence));
    assert(sequence.kind == kind);
    assert(sequence.role_count == expected_count);
    assert(strcmp(sequence.fact_id, expected_fact) == 0);
    assert(strcmp(sequence.evidence_maturity, expected_maturity) == 0);

    DBPartialOrderMatcher matcher = {0};
    assert(db_partial_order_matcher_initialize(&matcher, sequence.roles,
        sequence.role_count) == DB_PARTIAL_ORDER_RESULT_OK);
    for (size_t index = 0; index < sequence.role_count; ++index) {
        DBProtocolTransferMetadata metadata = metadata_for(
            &sequence.roles[index]);
        DBPartialOrderResult result = db_partial_order_matcher_accept(
            &matcher, &metadata);
        assert(result == (index + 1U == sequence.role_count ?
            DB_PARTIAL_ORDER_RESULT_COMPLETE : DB_PARTIAL_ORDER_RESULT_OK));
    }
    assert(db_partial_order_matcher_finish(&matcher) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);
}

static void
run_startup_orders(void)
{
    DBQualifiedSequence sequence = {0};
    assert(db_qualified_sequence_get(
        DB_QUALIFIED_SEQUENCE_STARTUP_FIRST_BURST, &sequence));
    assert(sequence.role_count == 15U);

    const size_t orders[][15] = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 9, 11, 12, 13, 14}
    };
    for (size_t order = 0; order < 2U; ++order) {
        DBPartialOrderMatcher matcher = {0};
        assert(db_partial_order_matcher_initialize(&matcher, sequence.roles,
            sequence.role_count) == DB_PARTIAL_ORDER_RESULT_OK);
        for (size_t index = 0; index < sequence.role_count; ++index) {
            DBProtocolTransferMetadata metadata = metadata_for(
                &sequence.roles[orders[order][index]]);
            DBPartialOrderResult result = db_partial_order_matcher_accept(
                &matcher, &metadata);
            assert(result == (index + 1U == sequence.role_count ?
                DB_PARTIAL_ORDER_RESULT_COMPLETE :
                DB_PARTIAL_ORDER_RESULT_OK));
        }
    }

    /* A different adjacent swap is not admitted by the observed DAG. */
    DBPartialOrderMatcher rejected = {0};
    assert(db_partial_order_matcher_initialize(&rejected, sequence.roles,
        sequence.role_count) == DB_PARTIAL_ORDER_RESULT_OK);
    DBProtocolTransferMetadata second = metadata_for(&sequence.roles[1]);
    assert(db_partial_order_matcher_accept(&rejected, &second) ==
        DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER);
}

static int
expected_transfers_equal(const ExpectedTransfer *first,
    const ExpectedTransfer *second)
{
    return first->direction == second->direction &&
        first->length == second->length;
}

static void
test_startup_adjacent_swaps(void)
{
    DBQualifiedSequence model = {0};
    assert(db_qualified_sequence_get(
        DB_QUALIFIED_SEQUENCE_STARTUP_FIRST_BURST, &model));
    for (size_t first = 0; first + 1U < model.role_count; ++first) {
        if (expected_transfers_equal(&expected_startup[first],
                &expected_startup[first + 1U])) {
            continue;
        }
        DBPartialOrderMatcher matcher = {0};
        assert(db_partial_order_matcher_initialize(&matcher, model.roles,
            model.role_count) == DB_PARTIAL_ORDER_RESULT_OK);
        for (size_t index = 0; index < first; ++index) {
            DBProtocolTransferMetadata metadata = metadata_for(
                &model.roles[index]);
            assert(db_partial_order_matcher_accept(&matcher, &metadata) ==
                DB_PARTIAL_ORDER_RESULT_OK);
        }
        const size_t swapped_indices[2] = {first + 1U, first};
        int rejected = 0;
        for (size_t index = 0; index < 2U; ++index) {
            DBProtocolTransferMetadata metadata = metadata_for(
                &model.roles[swapped_indices[index]]);
            DBPartialOrderResult result = db_partial_order_matcher_accept(
                &matcher, &metadata);
            if (result == DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER) {
                rejected = 1;
                break;
            }
            assert(result == DB_PARTIAL_ORDER_RESULT_OK);
        }
        for (size_t index = first + 2U;
             !rejected && index < model.role_count; ++index) {
            DBProtocolTransferMetadata metadata = metadata_for(
                &model.roles[index]);
            DBPartialOrderResult result = db_partial_order_matcher_accept(
                &matcher, &metadata);
            if (result == DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER) {
                rejected = 1;
                break;
            }
            assert(result == (index + 1U == model.role_count ?
                DB_PARTIAL_ORDER_RESULT_COMPLETE :
                DB_PARTIAL_ORDER_RESULT_OK));
        }
        if (first == 9U) {
            assert(!rejected);
            assert(matcher.state == DB_PARTIAL_ORDER_STATE_COMPLETE);
        } else {
            assert(rejected);
            assert(matcher.state == DB_PARTIAL_ORDER_STATE_FAILED);
        }
    }
}

int
main(void)
{
    assert(!db_qualified_sequence_get(DB_QUALIFIED_SEQUENCE_INVALID, NULL));
    DBQualifiedSequence invalid = {.role_count = 99U};
    assert(!db_qualified_sequence_get(DB_QUALIFIED_SEQUENCE_INVALID,
        &invalid));
    assert(invalid.role_count == 0U);

    assert_pinned_sequence(DB_QUALIFIED_SEQUENCE_STARTUP_FIRST_BURST,
        expected_startup,
        sizeof(expected_startup) / sizeof(expected_startup[0]));
    assert_pinned_sequence(DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX,
        expected_hotplug,
        sizeof(expected_hotplug) / sizeof(expected_hotplug[0]));
    assert_pinned_sequence(DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A,
        expected_hotunplug_a,
        sizeof(expected_hotunplug_a) / sizeof(expected_hotunplug_a[0]));
    assert_pinned_sequence(DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B,
        expected_hotunplug_b,
        sizeof(expected_hotunplug_b) / sizeof(expected_hotunplug_b[0]));

    run_startup_orders();
    test_startup_adjacent_swaps();
    run_chain(DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX, 24U,
        "FACT-HOTPLUG-PREFIX-001", "E2");
    run_chain(DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A, 29U,
        "FACT-HOTUNPLUG-PROFILES-001", "E2");
    run_chain(DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B, 29U,
        "FACT-HOTUNPLUG-PROFILES-001", "E2");

    assert(strcmp(db_qualified_sequence_kind_name(
        DB_QUALIFIED_SEQUENCE_STARTUP_FIRST_BURST),
        "startup-first-burst") == 0);
    assert(strcmp(db_qualified_sequence_kind_name(
        (DBQualifiedSequenceKind)99), "invalid") == 0);
    return 0;
}
