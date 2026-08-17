#include "partial_order_matcher.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

enum {
    TEST_ENDPOINT_OUT = 0x02,
    TEST_ENDPOINT_IN = 0x84
};

static DBPartialOrderRole
make_role(DBProtocolDirection direction, size_t length,
    uint32_t predecessor_mask)
{
    DBPartialOrderRole role = {
        .direction = direction,
        .endpoint = direction == DB_PROTOCOL_DIRECTION_IN ?
            TEST_ENDPOINT_IN : TEST_ENDPOINT_OUT,
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,
        .required_succeeded = 1,
        .allowed_length_count = 1,
        .allowed_lengths = {length, 0, 0, 0},
        .predecessor_mask = predecessor_mask
    };
    return role;
}

static DBProtocolTransferMetadata
make_transfer(DBProtocolDirection direction, size_t length)
{
    DBProtocolTransferMetadata transfer = {
        .direction = direction,
        .endpoint = direction == DB_PROTOCOL_DIRECTION_IN ?
            TEST_ENDPOINT_IN : TEST_ENDPOINT_OUT,
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,
        .succeeded = 1,
        .length = length,
        .in_prefix = {0, 0, 0, 0}
    };
    if (direction == DB_PROTOCOL_DIRECTION_IN &&
        length >= DB_PROTOCOL_IN_PREFIX_SIZE) {
        size_t body_length = length - DB_PROTOCOL_IN_PREFIX_SIZE;
        transfer.in_prefix[2] = (uint8_t)(body_length & 0xffU);
        transfer.in_prefix[3] = (uint8_t)((body_length >> 8U) & 0xffU);
    }
    return transfer;
}

static void
expect_sticky_failure(DBPartialOrderMatcher *matcher,
    const DBProtocolTransferMetadata *transfer,
    DBPartialOrderResult first_result)
{
    assert(db_partial_order_matcher_accept(matcher, transfer) ==
        first_result);
    assert(matcher->state == DB_PARTIAL_ORDER_STATE_FAILED);
    assert(db_partial_order_matcher_accept(matcher, transfer) ==
        DB_PARTIAL_ORDER_RESULT_FAILED);
    assert(db_partial_order_matcher_finish(matcher) ==
        DB_PARTIAL_ORDER_RESULT_FAILED);
}

static void
expect_invalid_model(const DBPartialOrderRole *roles, size_t role_count,
    DBPartialOrderResult expected)
{
    DBPartialOrderMatcher matcher;
    memset(&matcher, 0xa5, sizeof(matcher));
    assert(db_partial_order_matcher_initialize(&matcher, roles, role_count) ==
        expected);
    assert(matcher.state == DB_PARTIAL_ORDER_STATE_FAILED);
    assert(db_partial_order_matcher_finish(&matcher) ==
        DB_PARTIAL_ORDER_RESULT_FAILED);
}

static void
expect_corruption(DBPartialOrderMatcher *matcher)
{
    DBProtocolTransferMetadata transfer = make_transfer(
        DB_PROTOCOL_DIRECTION_OUT, 16);
    assert(db_partial_order_matcher_accept(matcher, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_CORRUPT_STATE);
    assert(matcher->state == DB_PARTIAL_ORDER_STATE_FAILED);
    assert(db_partial_order_matcher_accept(matcher, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_FAILED);
}

static void
test_transfer_metadata_validation(void)
{
    _Static_assert(DB_PROTOCOL_IN_PREFIX_SIZE == 4,
        "the metadata envelope must retain exactly four prefix bytes");
    _Static_assert(DB_PROTOCOL_MAX_TRANSFER_LENGTH == 65536,
        "the qualified transfer bound changed");

    DBProtocolTransferMetadata out = make_transfer(
        DB_PROTOCOL_DIRECTION_OUT, 1);
    assert(db_protocol_transfer_metadata_is_valid(&out));
    out.length = DB_PROTOCOL_MAX_TRANSFER_LENGTH;
    assert(db_protocol_transfer_metadata_is_valid(&out));

    DBProtocolTransferMetadata in = make_transfer(
        DB_PROTOCOL_DIRECTION_IN, 4);
    assert(db_protocol_transfer_metadata_is_valid(&in));
    in = make_transfer(DB_PROTOCOL_DIRECTION_IN, 549);
    assert(db_protocol_transfer_metadata_is_valid(&in));
    assert(in.in_prefix[2] == 0x21U);
    assert(in.in_prefix[3] == 0x02U);
    in = make_transfer(DB_PROTOCOL_DIRECTION_IN,
        DB_PROTOCOL_MAX_TRANSFER_LENGTH);
    assert(db_protocol_transfer_metadata_is_valid(&in));
    assert(in.in_prefix[2] == 0xfcU);
    assert(in.in_prefix[3] == 0xffU);

    assert(!db_protocol_transfer_metadata_is_valid(NULL));

    out = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 16);
    out.direction = DB_PROTOCOL_DIRECTION_INVALID;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    out.direction = (DBProtocolDirection)99;
    assert(!db_protocol_transfer_metadata_is_valid(&out));

    out = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 16);
    out.kind = DB_PROTOCOL_TRANSFER_KIND_INVALID;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    out.kind = (DBProtocolTransferKind)99;
    assert(!db_protocol_transfer_metadata_is_valid(&out));

    out = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 16);
    out.succeeded = 2;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    out = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 16);
    out.succeeded = 0;
    assert(db_protocol_transfer_metadata_is_valid(&out));

    out = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 16);
    out.endpoint = 0;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    out.endpoint = 0x12;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    out.endpoint = TEST_ENDPOINT_IN;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    in = make_transfer(DB_PROTOCOL_DIRECTION_IN, 16);
    in.endpoint = TEST_ENDPOINT_OUT;
    assert(!db_protocol_transfer_metadata_is_valid(&in));
    in.endpoint = 0x94;
    assert(!db_protocol_transfer_metadata_is_valid(&in));

    out = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 16);
    out.length = 0;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    out.length = (size_t)DB_PROTOCOL_MAX_TRANSFER_LENGTH + 1U;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    out = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 16);
    out.in_prefix[0] = 1;
    assert(!db_protocol_transfer_metadata_is_valid(&out));
    out = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 16);
    out.in_prefix[3] = 1;
    assert(!db_protocol_transfer_metadata_is_valid(&out));

    in = make_transfer(DB_PROTOCOL_DIRECTION_IN, 3);
    assert(!db_protocol_transfer_metadata_is_valid(&in));
    in = make_transfer(DB_PROTOCOL_DIRECTION_IN, 16);
    in.in_prefix[0] = 1;
    assert(!db_protocol_transfer_metadata_is_valid(&in));
    in = make_transfer(DB_PROTOCOL_DIRECTION_IN, 16);
    in.in_prefix[1] = 1;
    assert(!db_protocol_transfer_metadata_is_valid(&in));
    in = make_transfer(DB_PROTOCOL_DIRECTION_IN, 16);
    --in.in_prefix[2];
    assert(!db_protocol_transfer_metadata_is_valid(&in));
    in = make_transfer(DB_PROTOCOL_DIRECTION_IN, 16);
    ++in.in_prefix[2];
    assert(!db_protocol_transfer_metadata_is_valid(&in));
}

static void
test_model_validation(void)
{
    DBPartialOrderRole valid = make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0);
    DBPartialOrderMatcher matcher;
    assert(db_partial_order_matcher_initialize(NULL, &valid, 1) ==
        DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT);
    expect_invalid_model(NULL, 1,
        DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT);
    expect_invalid_model(&valid, 0, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);

    DBPartialOrderRole too_many[DB_PARTIAL_ORDER_MAX_ROLES + 1];
    for (size_t index = 0; index < DB_PARTIAL_ORDER_MAX_ROLES + 1U;
         ++index) {
        too_many[index] = valid;
    }
    expect_invalid_model(too_many, DB_PARTIAL_ORDER_MAX_ROLES + 1U,
        DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);

    DBPartialOrderRole bad = valid;
    bad.direction = DB_PROTOCOL_DIRECTION_INVALID;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.direction = (DBProtocolDirection)99;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.endpoint = 0;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.endpoint = TEST_ENDPOINT_IN;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.endpoint = 0x12;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.kind = DB_PROTOCOL_TRANSFER_KIND_INVALID;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.required_succeeded = 2;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.allowed_length_count = 0;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.allowed_length_count =
        DB_PARTIAL_ORDER_MAX_ALLOWED_LENGTHS + 1U;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.allowed_lengths[0] = 0;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.allowed_lengths[0] =
        (size_t)DB_PROTOCOL_MAX_TRANSFER_LENGTH + 1U;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad = valid;
    bad.allowed_lengths[1] = 32;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);

    bad = valid;
    bad.allowed_length_count = 2;
    bad.allowed_lengths[0] = 32;
    bad.allowed_lengths[1] = 16;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad.allowed_lengths[1] = 32;
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);

    bad = make_role(DB_PROTOCOL_DIRECTION_IN, 3, 0);
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);

    bad = valid;
    bad.predecessor_mask = UINT32_C(1);
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);
    bad.predecessor_mask = UINT32_C(2);
    expect_invalid_model(&bad, 1, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);

    DBPartialOrderRole cycle[2] = {
        make_role(DB_PROTOCOL_DIRECTION_OUT, 16, UINT32_C(2)),
        make_role(DB_PROTOCOL_DIRECTION_OUT, 32, UINT32_C(1))
    };
    expect_invalid_model(cycle, 2, DB_PARTIAL_ORDER_RESULT_INVALID_MODEL);

    DBPartialOrderRole maximum[DB_PARTIAL_ORDER_MAX_ROLES];
    for (size_t index = 0; index < DB_PARTIAL_ORDER_MAX_ROLES; ++index) {
        maximum[index] = make_role(DB_PROTOCOL_DIRECTION_OUT, index + 1U,
            index == 0U ? 0U : UINT32_C(1) << (index - 1U));
    }
    assert(db_partial_order_matcher_initialize(&matcher, maximum,
        DB_PARTIAL_ORDER_MAX_ROLES) == DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.all_roles_mask == UINT32_MAX);
    for (size_t index = 0; index < DB_PARTIAL_ORDER_MAX_ROLES; ++index) {
        DBProtocolTransferMetadata transfer = make_transfer(
            DB_PROTOCOL_DIRECTION_OUT, index + 1U);
        DBPartialOrderResult expected =
            index + 1U == DB_PARTIAL_ORDER_MAX_ROLES ?
            DB_PARTIAL_ORDER_RESULT_COMPLETE : DB_PARTIAL_ORDER_RESULT_OK;
        assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
            expected);
    }
    assert(db_partial_order_matcher_finish(&matcher) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);
}

static void
test_completion_and_finish(void)
{
    DBPartialOrderRole role = make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0);
    DBPartialOrderMatcher matcher;
    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.state == DB_PARTIAL_ORDER_STATE_WAITING);
    assert(matcher.consumed_transfer_count == 0);
    assert(matcher.candidate_count == 1);
    assert(matcher.candidate_masks[0] == 0);
    DBProtocolTransferMetadata transfer = make_transfer(
        DB_PROTOCOL_DIRECTION_OUT, 16);
    assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);
    assert(matcher.state == DB_PARTIAL_ORDER_STATE_COMPLETE);
    assert(matcher.consumed_transfer_count == 1);
    assert(matcher.candidate_count == 1);
    assert(matcher.candidate_masks[0] == 1);
    assert(db_partial_order_matcher_finish(&matcher) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);
    assert(db_partial_order_matcher_finish(&matcher) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);
    assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_ALREADY_COMPLETE);
    assert(matcher.state == DB_PARTIAL_ORDER_STATE_COMPLETE);

    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_finish(&matcher) ==
        DB_PARTIAL_ORDER_RESULT_INCOMPLETE);
    assert(matcher.state == DB_PARTIAL_ORDER_STATE_FAILED);
    assert(db_partial_order_matcher_finish(&matcher) ==
        DB_PARTIAL_ORDER_RESULT_FAILED);
    assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_FAILED);

    DBPartialOrderRole two[2] = {
        make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0),
        make_role(DB_PROTOCOL_DIRECTION_IN, 8, UINT32_C(1))
    };
    assert(db_partial_order_matcher_initialize(&matcher, two, 2) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.state == DB_PARTIAL_ORDER_STATE_IN_PROGRESS);
    assert(db_partial_order_matcher_finish(&matcher) ==
        DB_PARTIAL_ORDER_RESULT_INCOMPLETE);
    assert(matcher.state == DB_PARTIAL_ORDER_STATE_FAILED);
    assert(db_partial_order_matcher_finish(NULL) ==
        DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT);
}

static int
permutation_respects_dependencies(const size_t permutation[4])
{
    size_t position[4] = {0};
    for (size_t index = 0; index < 4; ++index) {
        position[permutation[index]] = index;
    }
    return position[0] < position[2] && position[1] < position[3];
}

static void
test_every_four_role_permutation(void)
{
    DBPartialOrderRole roles[4] = {
        make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0),
        make_role(DB_PROTOCOL_DIRECTION_OUT, 32, 0),
        make_role(DB_PROTOCOL_DIRECTION_IN, 8, UINT32_C(1) << 0U),
        make_role(DB_PROTOCOL_DIRECTION_IN, 9, UINT32_C(1) << 1U)
    };
    size_t valid_permutations = 0;
    size_t rejected_permutations = 0;
    for (size_t a = 0; a < 4; ++a) {
        for (size_t b = 0; b < 4; ++b) {
            if (b == a) {
                continue;
            }
            for (size_t c = 0; c < 4; ++c) {
                if (c == a || c == b) {
                    continue;
                }
                size_t d = 6U - a - b - c;
                size_t permutation[4] = {a, b, c, d};
                DBPartialOrderMatcher matcher;
                assert(db_partial_order_matcher_initialize(&matcher, roles,
                    4) == DB_PARTIAL_ORDER_RESULT_OK);
                int rejected = 0;
                for (size_t index = 0; index < 4; ++index) {
                    size_t role_index = permutation[index];
                    DBProtocolTransferMetadata transfer = make_transfer(
                        roles[role_index].direction,
                        roles[role_index].allowed_lengths[0]);
                    DBPartialOrderResult result =
                        db_partial_order_matcher_accept(&matcher, &transfer);
                    if (result == DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER) {
                        rejected = 1;
                        assert(matcher.state ==
                            DB_PARTIAL_ORDER_STATE_FAILED);
                        break;
                    }
                    assert(result == (index == 3U ?
                        DB_PARTIAL_ORDER_RESULT_COMPLETE :
                        DB_PARTIAL_ORDER_RESULT_OK));
                }
                if (permutation_respects_dependencies(permutation)) {
                    assert(!rejected);
                    assert(matcher.state == DB_PARTIAL_ORDER_STATE_COMPLETE);
                    ++valid_permutations;
                } else {
                    assert(rejected);
                    ++rejected_permutations;
                }
            }
        }
    }
    assert(valid_permutations == 6);
    assert(rejected_permutations == 18);
}

static void
test_ambiguous_roles_are_not_greedy(void)
{
    DBPartialOrderRole roles[4] = {
        make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0),
        make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0),
        make_role(DB_PROTOCOL_DIRECTION_IN, 8, UINT32_C(1) << 0U),
        make_role(DB_PROTOCOL_DIRECTION_IN, 9, UINT32_C(1) << 1U)
    };
    DBProtocolTransferMetadata a = make_transfer(
        DB_PROTOCOL_DIRECTION_OUT, 16);
    DBProtocolTransferMetadata b = make_transfer(
        DB_PROTOCOL_DIRECTION_IN, 8);
    DBProtocolTransferMetadata c = make_transfer(
        DB_PROTOCOL_DIRECTION_IN, 9);
    DBPartialOrderMatcher matcher;

    assert(db_partial_order_matcher_initialize(&matcher, roles, 4) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &a) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.candidate_count == 2);
    assert(matcher.candidate_masks[0] == UINT32_C(1));
    assert(matcher.candidate_masks[1] == UINT32_C(2));
    assert(db_partial_order_matcher_accept(&matcher, &b) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.candidate_count == 1);
    assert(matcher.candidate_masks[0] == (UINT32_C(1) | UINT32_C(4)));
    assert(db_partial_order_matcher_accept(&matcher, &a) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &c) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);

    assert(db_partial_order_matcher_initialize(&matcher, roles, 4) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &a) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &c) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.candidate_count == 1);
    assert(matcher.candidate_masks[0] == (UINT32_C(2) | UINT32_C(8)));
    assert(db_partial_order_matcher_accept(&matcher, &a) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &b) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);

    assert(db_partial_order_matcher_initialize(&matcher, roles, 4) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &a) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.candidate_count == 2);
    assert(db_partial_order_matcher_accept(&matcher, &a) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.candidate_count == 1);
    assert(matcher.candidate_masks[0] == UINT32_C(3));
    assert(db_partial_order_matcher_accept(&matcher, &b) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &c) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);
}

static void
test_allowed_lengths_and_exact_metadata(void)
{
    DBPartialOrderRole role = make_role(DB_PROTOCOL_DIRECTION_IN, 8, 0);
    role.allowed_length_count = DB_PARTIAL_ORDER_MAX_ALLOWED_LENGTHS;
    role.allowed_lengths[0] = 8;
    role.allowed_lengths[1] = 16;
    role.allowed_lengths[2] = 38;
    role.allowed_lengths[3] = 549;
    for (size_t index = 0; index < role.allowed_length_count; ++index) {
        DBPartialOrderMatcher matcher;
        assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
            DB_PARTIAL_ORDER_RESULT_OK);
        DBProtocolTransferMetadata transfer = make_transfer(
            DB_PROTOCOL_DIRECTION_IN, role.allowed_lengths[index]);
        assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
            DB_PARTIAL_ORDER_RESULT_COMPLETE);
    }

    DBPartialOrderMatcher matcher;
    DBProtocolTransferMetadata transfer;
    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    transfer = make_transfer(DB_PROTOCOL_DIRECTION_IN, 9);
    expect_sticky_failure(&matcher, &transfer,
        DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER);

    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    transfer = make_transfer(DB_PROTOCOL_DIRECTION_IN, 8);
    transfer.endpoint = 0x85;
    expect_sticky_failure(&matcher, &transfer,
        DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER);

    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    transfer = make_transfer(DB_PROTOCOL_DIRECTION_OUT, 8);
    expect_sticky_failure(&matcher, &transfer,
        DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER);

    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    transfer = make_transfer(DB_PROTOCOL_DIRECTION_IN, 8);
    transfer.succeeded = 0;
    expect_sticky_failure(&matcher, &transfer,
        DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER);

    role.required_succeeded = 0;
    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    transfer = make_transfer(DB_PROTOCOL_DIRECTION_IN, 8);
    transfer.succeeded = 0;
    assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_COMPLETE);

    role.required_succeeded = 1;
    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    transfer = make_transfer(DB_PROTOCOL_DIRECTION_IN, 8);
    ++transfer.in_prefix[2];
    expect_sticky_failure(&matcher, &transfer,
        DB_PARTIAL_ORDER_RESULT_INVALID_TRANSFER);

    assert(db_partial_order_matcher_initialize(&matcher, &role, 1) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    expect_sticky_failure(&matcher, NULL,
        DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT);
    assert(db_partial_order_matcher_accept(NULL, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_INVALID_ARGUMENT);
}

static void
test_linear_sequence_mutations(void)
{
    DBPartialOrderRole roles[5] = {
        make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0),
        make_role(DB_PROTOCOL_DIRECTION_IN, 8, UINT32_C(1) << 0U),
        make_role(DB_PROTOCOL_DIRECTION_OUT, 32, UINT32_C(1) << 1U),
        make_role(DB_PROTOCOL_DIRECTION_IN, 9, UINT32_C(1) << 2U),
        make_role(DB_PROTOCOL_DIRECTION_OUT, 48, UINT32_C(1) << 3U)
    };
    for (size_t missing = 0; missing < 5; ++missing) {
        DBPartialOrderMatcher matcher;
        assert(db_partial_order_matcher_initialize(&matcher, roles, 5) ==
            DB_PARTIAL_ORDER_RESULT_OK);
        int rejected = 0;
        for (size_t index = 0; index < 5; ++index) {
            if (index == missing) {
                continue;
            }
            DBProtocolTransferMetadata transfer = make_transfer(
                roles[index].direction, roles[index].allowed_lengths[0]);
            DBPartialOrderResult result =
                db_partial_order_matcher_accept(&matcher, &transfer);
            if (result == DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER) {
                rejected = 1;
                break;
            }
            assert(result == DB_PARTIAL_ORDER_RESULT_OK);
        }
        if (missing == 4U) {
            assert(!rejected);
            assert(db_partial_order_matcher_finish(&matcher) ==
                DB_PARTIAL_ORDER_RESULT_INCOMPLETE);
        } else {
            assert(rejected);
            assert(matcher.state == DB_PARTIAL_ORDER_STATE_FAILED);
        }
    }

    for (size_t duplicated = 0; duplicated < 5; ++duplicated) {
        DBPartialOrderMatcher matcher;
        assert(db_partial_order_matcher_initialize(&matcher, roles, 5) ==
            DB_PARTIAL_ORDER_RESULT_OK);
        for (size_t index = 0; index <= duplicated; ++index) {
            DBProtocolTransferMetadata transfer = make_transfer(
                roles[index].direction, roles[index].allowed_lengths[0]);
            DBPartialOrderResult expected = index == 4U ?
                DB_PARTIAL_ORDER_RESULT_COMPLETE :
                DB_PARTIAL_ORDER_RESULT_OK;
            assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
                expected);
        }
        DBProtocolTransferMetadata duplicate = make_transfer(
            roles[duplicated].direction,
            roles[duplicated].allowed_lengths[0]);
        if (duplicated == 4U) {
            assert(db_partial_order_matcher_accept(&matcher, &duplicate) ==
                DB_PARTIAL_ORDER_RESULT_ALREADY_COMPLETE);
            assert(matcher.state == DB_PARTIAL_ORDER_STATE_COMPLETE);
        } else {
            expect_sticky_failure(&matcher, &duplicate,
                DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER);
        }
    }

    for (size_t insertion = 0; insertion <= 5; ++insertion) {
        DBPartialOrderMatcher matcher;
        assert(db_partial_order_matcher_initialize(&matcher, roles, 5) ==
            DB_PARTIAL_ORDER_RESULT_OK);
        for (size_t index = 0; index < insertion; ++index) {
            DBProtocolTransferMetadata transfer = make_transfer(
                roles[index].direction, roles[index].allowed_lengths[0]);
            DBPartialOrderResult expected = index == 4U ?
                DB_PARTIAL_ORDER_RESULT_COMPLETE :
                DB_PARTIAL_ORDER_RESULT_OK;
            assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
                expected);
        }
        DBProtocolTransferMetadata inserted = make_transfer(
            DB_PROTOCOL_DIRECTION_OUT, 64);
        DBPartialOrderResult expected = insertion == 5U ?
            DB_PARTIAL_ORDER_RESULT_ALREADY_COMPLETE :
            DB_PARTIAL_ORDER_RESULT_UNEXPECTED_TRANSFER;
        assert(db_partial_order_matcher_accept(&matcher, &inserted) ==
            expected);
        assert(matcher.state == (insertion == 5U ?
            DB_PARTIAL_ORDER_STATE_COMPLETE :
            DB_PARTIAL_ORDER_STATE_FAILED));
    }
}

static void
test_candidate_limit(void)
{
    DBPartialOrderRole eight[DB_PARTIAL_ORDER_MAX_CANDIDATES];
    for (size_t index = 0; index < DB_PARTIAL_ORDER_MAX_CANDIDATES; ++index) {
        eight[index] = make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0);
    }
    DBProtocolTransferMetadata transfer = make_transfer(
        DB_PROTOCOL_DIRECTION_OUT, 16);
    DBPartialOrderMatcher matcher;
    assert(db_partial_order_matcher_initialize(&matcher, eight,
        DB_PARTIAL_ORDER_MAX_CANDIDATES) == DB_PARTIAL_ORDER_RESULT_OK);
    assert(db_partial_order_matcher_accept(&matcher, &transfer) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    assert(matcher.candidate_count == DB_PARTIAL_ORDER_MAX_CANDIDATES);
    for (size_t index = 0; index < DB_PARTIAL_ORDER_MAX_CANDIDATES; ++index) {
        assert(matcher.candidate_masks[index] ==
            (UINT32_C(1) << index));
    }
    expect_sticky_failure(&matcher, &transfer,
        DB_PARTIAL_ORDER_RESULT_CANDIDATE_LIMIT);

    DBPartialOrderRole nine[DB_PARTIAL_ORDER_MAX_CANDIDATES + 1];
    for (size_t index = 0; index < DB_PARTIAL_ORDER_MAX_CANDIDATES + 1U;
         ++index) {
        nine[index] = make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0);
    }
    assert(db_partial_order_matcher_initialize(&matcher, nine,
        DB_PARTIAL_ORDER_MAX_CANDIDATES + 1U) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    expect_sticky_failure(&matcher, &transfer,
        DB_PARTIAL_ORDER_RESULT_CANDIDATE_LIMIT);
}

static DBPartialOrderMatcher
make_two_root_matcher(void)
{
    DBPartialOrderRole roles[2] = {
        make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0),
        make_role(DB_PROTOCOL_DIRECTION_OUT, 32, 0)
    };
    DBPartialOrderMatcher matcher;
    assert(db_partial_order_matcher_initialize(&matcher, roles, 2) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    return matcher;
}

static void
test_corruption_invariants(void)
{
    DBPartialOrderMatcher matcher = make_two_root_matcher();
    matcher.state = (DBPartialOrderState)99;
    expect_corruption(&matcher);

    matcher = make_two_root_matcher();
    matcher.role_count = 0;
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.role_count = DB_PARTIAL_ORDER_MAX_ROLES + 1U;
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.all_roles_mask ^= UINT32_C(1);
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.consumed_transfer_count = 3;
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.candidate_count = 0;
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.candidate_count = DB_PARTIAL_ORDER_MAX_CANDIDATES + 1U;
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.candidate_masks[0] = UINT32_C(4);
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.candidate_masks[1] = UINT32_C(1);
    expect_corruption(&matcher);

    matcher = make_two_root_matcher();
    matcher.state = DB_PARTIAL_ORDER_STATE_IN_PROGRESS;
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.state = DB_PARTIAL_ORDER_STATE_COMPLETE;
    expect_corruption(&matcher);

    DBProtocolTransferMetadata first = make_transfer(
        DB_PROTOCOL_DIRECTION_OUT, 16);
    matcher = make_two_root_matcher();
    assert(db_partial_order_matcher_accept(&matcher, &first) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    matcher.consumed_transfer_count = 0;
    expect_corruption(&matcher);

    matcher = make_two_root_matcher();
    matcher.state = DB_PARTIAL_ORDER_STATE_IN_PROGRESS;
    matcher.consumed_transfer_count = 1;
    matcher.candidate_count = 2;
    matcher.candidate_masks[0] = UINT32_C(1);
    matcher.candidate_masks[1] = UINT32_C(1);
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.state = DB_PARTIAL_ORDER_STATE_IN_PROGRESS;
    matcher.consumed_transfer_count = 1;
    matcher.candidate_count = 2;
    matcher.candidate_masks[0] = UINT32_C(2);
    matcher.candidate_masks[1] = UINT32_C(1);
    expect_corruption(&matcher);

    matcher = make_two_root_matcher();
    matcher.roles[0].allowed_length_count = 0;
    expect_corruption(&matcher);
    matcher = make_two_root_matcher();
    matcher.roles[0].predecessor_mask = UINT32_C(2);
    matcher.roles[1].predecessor_mask = UINT32_C(1);
    expect_corruption(&matcher);

    DBPartialOrderRole dependent[2] = {
        make_role(DB_PROTOCOL_DIRECTION_OUT, 16, 0),
        make_role(DB_PROTOCOL_DIRECTION_OUT, 32, UINT32_C(1))
    };
    assert(db_partial_order_matcher_initialize(&matcher, dependent, 2) ==
        DB_PARTIAL_ORDER_RESULT_OK);
    matcher.state = DB_PARTIAL_ORDER_STATE_IN_PROGRESS;
    matcher.consumed_transfer_count = 1;
    matcher.candidate_masks[0] = UINT32_C(2);
    expect_corruption(&matcher);

    matcher = make_two_root_matcher();
    matcher.state = DB_PARTIAL_ORDER_STATE_COMPLETE;
    matcher.consumed_transfer_count = 2;
    matcher.candidate_masks[0] = UINT32_C(1);
    expect_corruption(&matcher);

    matcher = make_two_root_matcher();
    matcher.state = DB_PARTIAL_ORDER_STATE_FAILED;
    matcher.role_count = SIZE_MAX;
    assert(db_partial_order_matcher_accept(&matcher, &first) ==
        DB_PARTIAL_ORDER_RESULT_FAILED);
}

static void
test_names(void)
{
    assert(strcmp(db_partial_order_state_name(
        DB_PARTIAL_ORDER_STATE_WAITING), "waiting") == 0);
    assert(strcmp(db_partial_order_state_name(
        DB_PARTIAL_ORDER_STATE_IN_PROGRESS), "in-progress") == 0);
    assert(strcmp(db_partial_order_state_name(
        DB_PARTIAL_ORDER_STATE_COMPLETE), "complete") == 0);
    assert(strcmp(db_partial_order_state_name(
        DB_PARTIAL_ORDER_STATE_FAILED), "failed") == 0);
    assert(strcmp(db_partial_order_state_name((DBPartialOrderState)99),
        "invalid-state") == 0);

    const char *expected[] = {
        "ok",
        "complete",
        "incomplete",
        "invalid-argument",
        "invalid-model",
        "invalid-transfer",
        "unexpected-transfer",
        "candidate-limit",
        "corrupt-state",
        "failed",
        "already-complete"
    };
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]);
         ++index) {
        assert(strcmp(db_partial_order_result_name(
            (DBPartialOrderResult)index), expected[index]) == 0);
    }
    assert(strcmp(db_partial_order_result_name((DBPartialOrderResult)99),
        "invalid-result") == 0);
}

int
main(void)
{
    test_transfer_metadata_validation();
    test_model_validation();
    test_completion_and_finish();
    test_every_four_role_permutation();
    test_ambiguous_roles_are_not_greedy();
    test_allowed_lengths_and_exact_metadata();
    test_linear_sequence_mutations();
    test_candidate_limit();
    test_corruption_invariants();
    test_names();
    return 0;
}
