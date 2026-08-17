#include "fake_transport.h"
#include "qualified_sequences.h"
#include "transition_parser.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    DBFakeTransport fake;
    DBTransport transport;
    DBMachine machine;
} Fixture;

static DBMachineDeviceIdentity
identity(void)
{
    return (DBMachineDeviceIdentity) {
        DB_MACHINE_VENDOR_ID,
        DB_MACHINE_PRODUCT_ID,
        DB_MACHINE_DEVICE_REVISION
    };
}

static DBMachineTopology
topology(void)
{
    return (DBMachineTopology) {
        .display_interface = DB_MACHINE_DISPLAY_INTERFACE,
        .display_class = 0xff,
        .display_subclass = 0,
        .display_protocol = 3,
        .auxiliary_interface = DB_MACHINE_AUXILIARY_INTERFACE,
        .auxiliary_endpoint_count = 0,
        .endpoint_out = DB_MACHINE_ENDPOINT_OUT,
        .endpoint_out_type = DB_MACHINE_TRANSFER_TYPE_BULK,
        .endpoint_out_max_packet = DB_MACHINE_MAX_PACKET_SIZE,
        .endpoint_in = DB_MACHINE_ENDPOINT_IN,
        .endpoint_in_type = DB_MACHINE_TRANSFER_TYPE_BULK,
        .endpoint_in_max_packet = DB_MACHINE_MAX_PACKET_SIZE,
        .endpoint_out_burst_packets = 1,
        .endpoint_in_burst_packets = 1,
        .endpoint_out_streams = 0,
        .endpoint_in_streams = 0
    };
}

static void
fixture_initialize(Fixture *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    db_fake_transport_initialize(&fixture->fake, &fixture->transport);
    db_machine_initialize(&fixture->machine, &fixture->transport);
    DBMachineDeviceIdentity exact_identity = identity();
    DBMachineTopology exact_topology = topology();
    assert(db_machine_attach(&fixture->machine, &exact_identity) ==
        DB_MACHINE_OK);
    assert(db_machine_verify_topology(&fixture->machine, &exact_topology) ==
        DB_MACHINE_OK);
    assert(db_machine_is_exact_verified(&fixture->machine));
}

static DBProtocolTransferMetadata
metadata_for(const DBPartialOrderRole *role)
{
    DBProtocolTransferMetadata metadata = {
        .direction = role->direction,
        .endpoint = role->endpoint,
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,
        .succeeded = 1U,
        .length = role->allowed_lengths[0]
    };
    if (metadata.direction == DB_PROTOCOL_DIRECTION_IN) {
        size_t body_length = metadata.length - DB_PROTOCOL_IN_PREFIX_SIZE;
        metadata.in_prefix[2] = (uint8_t)(body_length & 0xffU);
        metadata.in_prefix[3] = (uint8_t)((body_length >> 8U) & 0xffU);
    }
    return metadata;
}

static DBQualifiedSequence
sequence(DBQualifiedSequenceKind kind)
{
    DBQualifiedSequence result = {0};
    assert(db_qualified_sequence_get(kind, &result));
    return result;
}

static void
accept_sequence(DBTransitionParser *parser,
    const DBQualifiedSequence *model, size_t begin, size_t end)
{
    assert(end <= model->role_count);
    for (size_t index = begin; index < end; ++index) {
        DBProtocolTransferMetadata metadata = metadata_for(
            &model->roles[index]);
        DBTransitionResult result = db_transition_parser_accept(parser,
            &metadata);
        assert(result == (index + 1U == model->role_count ?
            DB_TRANSITION_RESULT_COMPLETE : DB_TRANSITION_RESULT_OK));
    }
}

static void
test_complete_profiles(void)
{
    Fixture fixture;
    fixture_initialize(&fixture);

    DBTransitionParser hotplug = {0};
    assert(db_transition_parser_initialize(&hotplug,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    assert(hotplug.state == DB_TRANSITION_STATE_WAITING);
    assert(db_transition_parser_stage(&hotplug) ==
        DB_TRANSITION_STAGE_WAITING);
    DBQualifiedSequence plug = sequence(
        DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX);
    accept_sequence(&hotplug, &plug, 0, 14);
    assert(db_transition_parser_stage(&hotplug) ==
        DB_TRANSITION_STAGE_PREFIX_BEFORE_15);
    accept_sequence(&hotplug, &plug, 14, 15);
    assert(db_transition_parser_stage(&hotplug) ==
        DB_TRANSITION_STAGE_FIRST_15_OBSERVED);
    accept_sequence(&hotplug, &plug, 15, plug.role_count);
    assert(hotplug.state == DB_TRANSITION_STATE_COMPLETE);
    assert(db_transition_parser_stage(&hotplug) ==
        DB_TRANSITION_STAGE_COMPLETE);
    assert(db_transition_parser_observed_profile(&hotplug) ==
        DB_TRANSITION_PROFILE_HOTPLUG_PREFIX);
    assert(db_transition_parser_finish(&hotplug) ==
        DB_TRANSITION_RESULT_COMPLETE);
    DBProtocolTransferMetadata extra = metadata_for(&plug.roles[0]);
    assert(db_transition_parser_accept(&hotplug, &extra) ==
        DB_TRANSITION_RESULT_ALREADY_COMPLETE);

    DBQualifiedSequence profiles[] = {
        sequence(DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A),
        sequence(DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B)
    };
    const DBTransitionObservedProfile expected[] = {
        DB_TRANSITION_PROFILE_HOTUNPLUG_A,
        DB_TRANSITION_PROFILE_HOTUNPLUG_B
    };
    for (size_t profile = 0; profile < 2U; ++profile) {
        DBTransitionParser parser = {0};
        assert(db_transition_parser_initialize(&parser,
            DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            &fixture.machine) == DB_TRANSITION_RESULT_OK);
        assert(db_transition_parser_observed_profile(&parser) ==
            DB_TRANSITION_PROFILE_NONE);
        accept_sequence(&parser, &profiles[profile], 0,
            profiles[profile].role_count);
        assert(parser.state == DB_TRANSITION_STATE_COMPLETE);
        assert(db_transition_parser_observed_profile(&parser) ==
            expected[profile]);
        assert(db_transition_parser_stage(&parser) ==
            DB_TRANSITION_STAGE_NOT_APPLICABLE);
    }

    assert(fixture.fake.write_attempt_count == 0U);
    assert(fixture.fake.read_attempt_count == 0U);
    assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
}

static void
test_binding(void)
{
    DBTransitionParser parser = {0};
    assert(db_transition_parser_initialize(NULL,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, NULL) ==
        DB_TRANSITION_RESULT_INVALID_ARGUMENT);
    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_INVALID, NULL) ==
        DB_TRANSITION_RESULT_INVALID_KIND);
    assert(parser.state == DB_TRANSITION_STATE_FAILED);

    Fixture fixture;
    fixture_initialize(&fixture);
    DBMachine unverified = fixture.machine;
    unverified.state = DB_MACHINE_ATTACHED;
    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &unverified) ==
        DB_TRANSITION_RESULT_BINDING_REQUIRED);
    DBMachine wrong_identity = fixture.machine;
    ++wrong_identity.identity.revision;
    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &wrong_identity) ==
        DB_TRANSITION_RESULT_BINDING_REQUIRED);
    DBMachine wrong_topology = fixture.machine;
    wrong_topology.topology.endpoint_in = 0x85U;
    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &wrong_topology) ==
        DB_TRANSITION_RESULT_BINDING_REQUIRED);
    DBMachine wrong_transport = fixture.machine;
    DBTransport nonfake = fixture.transport;
    nonfake.kind = (DBTransportKind)99;
    wrong_transport.transport = &nonfake;
    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &wrong_transport) ==
        DB_TRANSITION_RESULT_BINDING_REQUIRED);

    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    DBQualifiedSequence plug = sequence(
        DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX);
    DBProtocolTransferMetadata first = metadata_for(&plug.roles[0]);
    assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
    assert(db_transition_parser_accept(&parser, &first) ==
        DB_TRANSITION_RESULT_BINDING_LOST);
    assert(parser.state == DB_TRANSITION_STATE_FAILED);
    assert(db_transition_parser_accept(&parser, &first) ==
        DB_TRANSITION_RESULT_FAILED);

    DBMachineDeviceIdentity exact_identity = identity();
    DBMachineTopology exact_topology = topology();
    uint64_t previous_generation = fixture.machine.generation;
    assert(db_machine_attach(&fixture.machine, &exact_identity) ==
        DB_MACHINE_OK);
    assert(db_machine_verify_topology(&fixture.machine, &exact_topology) ==
        DB_MACHINE_OK);
    assert(fixture.machine.generation != 0U);
    assert(fixture.machine.generation != previous_generation);
    assert(db_transition_parser_finish(&parser) ==
        DB_TRANSITION_RESULT_FAILED);
    assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);

    /* Reinitializing the same machine object cannot recreate a bound epoch. */
    fixture_initialize(&fixture);
    DBTransitionParser aba_parser = {0};
    assert(db_transition_parser_initialize(&aba_parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    uint64_t bound_generation = fixture.machine.generation;
    db_machine_initialize(&fixture.machine, &fixture.transport);
    assert(db_machine_attach(&fixture.machine, &exact_identity) ==
        DB_MACHINE_OK);
    assert(db_machine_verify_topology(&fixture.machine, &exact_topology) ==
        DB_MACHINE_OK);
    assert(fixture.machine.generation != bound_generation);
    assert(db_transition_parser_accept(&aba_parser, &first) ==
        DB_TRANSITION_RESULT_BINDING_LOST);
    assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);

    /* Reinitializing the transport cannot recreate its bound lifecycle. */
    fixture_initialize(&fixture);
    DBTransitionParser transport_aba_parser = {0};
    assert(db_transition_parser_initialize(&transport_aba_parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    uint64_t bound_transport_epoch =
        transport_aba_parser.transport_lifecycle_epoch;
    db_fake_transport_initialize(&fixture.fake, &fixture.transport);
    assert(db_transport_open(&fixture.transport) == DB_TRANSPORT_OK);
    assert(db_transport_lifecycle_epoch(&fixture.transport) != 0U);
    assert(db_transport_lifecycle_epoch(&fixture.transport) !=
        bound_transport_epoch);
    assert(db_transition_parser_accept(&transport_aba_parser, &first) ==
        DB_TRANSITION_RESULT_BINDING_LOST);
    assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
}

typedef enum {
    DB_BINDING_DISRUPTION_DETACH = 0,
    DB_BINDING_DISRUPTION_DISCONNECT,
    DB_BINDING_DISRUPTION_DISCONNECT_RECONNECT,
    DB_BINDING_DISRUPTION_CLOSE_OPEN
} DBBindingDisruption;

static void
test_binding_loss_for_sequence(DBTransitionKind parser_kind,
    DBQualifiedSequenceKind sequence_kind, DBBindingDisruption disruption)
{
    DBQualifiedSequence model = sequence(sequence_kind);
    for (size_t prefix = 0; prefix <= model.role_count; ++prefix) {
        Fixture fixture;
        fixture_initialize(&fixture);
        DBTransitionParser parser = {0};
        assert(db_transition_parser_initialize(&parser, parser_kind,
            &fixture.machine) == DB_TRANSITION_RESULT_OK);
        uint64_t bound_transport_epoch = parser.transport_lifecycle_epoch;
        assert(bound_transport_epoch != 0U);
        accept_sequence(&parser, &model, 0, prefix);
        if (disruption == DB_BINDING_DISRUPTION_DETACH) {
            assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
        } else if (disruption == DB_BINDING_DISRUPTION_DISCONNECT) {
            db_fake_transport_disconnect(&fixture.fake);
            assert(!db_machine_is_exact_verified(&fixture.machine));
        } else {
            if (disruption == DB_BINDING_DISRUPTION_DISCONNECT_RECONNECT) {
                db_fake_transport_disconnect(&fixture.fake);
                db_fake_transport_reconnect(&fixture.fake);
            } else {
                assert(disruption == DB_BINDING_DISRUPTION_CLOSE_OPEN);
                db_transport_close(&fixture.transport);
            }
            assert(db_transport_open(&fixture.transport) == DB_TRANSPORT_OK);
            assert(db_transport_is_open(&fixture.transport));
            assert(db_transport_lifecycle_epoch(&fixture.transport) != 0U);
            assert(db_transport_lifecycle_epoch(&fixture.transport) !=
                bound_transport_epoch);
            assert(!db_machine_is_exact_verified(&fixture.machine));
        }
        assert(db_transition_parser_observed_profile(&parser) ==
            DB_TRANSITION_PROFILE_NONE);
        assert(db_transition_parser_stage(&parser) ==
            DB_TRANSITION_STAGE_NOT_APPLICABLE);
        if (prefix == model.role_count) {
            assert(db_transition_parser_finish(&parser) ==
                DB_TRANSITION_RESULT_BINDING_LOST);
        } else {
            DBProtocolTransferMetadata next = metadata_for(
                &model.roles[prefix]);
            assert(db_transition_parser_accept(&parser, &next) ==
                DB_TRANSITION_RESULT_BINDING_LOST);
        }
        assert(parser.state == DB_TRANSITION_STATE_FAILED);
        if (disruption != DB_BINDING_DISRUPTION_DETACH) {
            assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
        }
    }
}

static void
test_binding_loss_at_every_prefix(void)
{
    const struct {
        DBTransitionKind parser_kind;
        DBQualifiedSequenceKind sequence_kind;
    } cases[] = {
        {DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX,
            DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX},
        {DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A},
        {DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B}
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        test_binding_loss_for_sequence(cases[index].parser_kind,
            cases[index].sequence_kind, DB_BINDING_DISRUPTION_DETACH);
        test_binding_loss_for_sequence(cases[index].parser_kind,
            cases[index].sequence_kind, DB_BINDING_DISRUPTION_DISCONNECT);
        test_binding_loss_for_sequence(cases[index].parser_kind,
            cases[index].sequence_kind,
            DB_BINDING_DISRUPTION_DISCONNECT_RECONNECT);
        test_binding_loss_for_sequence(cases[index].parser_kind,
            cases[index].sequence_kind, DB_BINDING_DISRUPTION_CLOSE_OPEN);
    }
}

static void
test_each_role_rejects_a_valid_wrong_length(void)
{
    const struct {
        DBTransitionKind parser_kind;
        DBQualifiedSequenceKind sequence_kind;
    } cases[] = {
        {DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX,
            DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX},
        {DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A},
        {DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B}
    };
    for (size_t case_index = 0;
         case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
        DBQualifiedSequence model = sequence(cases[case_index].sequence_kind);
        for (size_t target = 0; target < model.role_count; ++target) {
            Fixture fixture;
            fixture_initialize(&fixture);
            DBTransitionParser parser = {0};
            assert(db_transition_parser_initialize(&parser,
                cases[case_index].parser_kind, &fixture.machine) ==
                DB_TRANSITION_RESULT_OK);
            accept_sequence(&parser, &model, 0, target);
            DBProtocolTransferMetadata wrong = metadata_for(
                &model.roles[target]);
            if (wrong.direction == DB_PROTOCOL_DIRECTION_OUT) {
                wrong.length += 16U;
            } else {
                ++wrong.length;
                size_t body_length =
                    wrong.length - DB_PROTOCOL_IN_PREFIX_SIZE;
                wrong.in_prefix[2] = (uint8_t)(body_length & 0xffU);
                wrong.in_prefix[3] =
                    (uint8_t)((body_length >> 8U) & 0xffU);
            }
            assert(db_transition_parser_accept(&parser, &wrong) ==
                DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER);
            assert(parser.state == DB_TRANSITION_STATE_FAILED);
            assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
        }
    }
}

static int
roles_have_same_metadata(const DBPartialOrderRole *first,
    const DBPartialOrderRole *second)
{
    return first->direction == second->direction &&
        first->endpoint == second->endpoint &&
        first->kind == second->kind &&
        first->required_succeeded == second->required_succeeded &&
        first->allowed_length_count == 1U &&
        second->allowed_length_count == 1U &&
        first->allowed_lengths[0] == second->allowed_lengths[0];
}

static void
test_every_observable_adjacent_swap_is_rejected(void)
{
    const struct {
        DBTransitionKind parser_kind;
        DBQualifiedSequenceKind sequence_kind;
    } cases[] = {
        {DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX,
            DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX},
        {DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A},
        {DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B}
    };
    for (size_t case_index = 0;
         case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
        DBQualifiedSequence model = sequence(cases[case_index].sequence_kind);
        for (size_t first_index = 0;
             first_index + 1U < model.role_count; ++first_index) {
            if (roles_have_same_metadata(&model.roles[first_index],
                    &model.roles[first_index + 1U])) {
                continue;
            }
            Fixture fixture;
            fixture_initialize(&fixture);
            DBTransitionParser parser = {0};
            assert(db_transition_parser_initialize(&parser,
                cases[case_index].parser_kind, &fixture.machine) ==
                DB_TRANSITION_RESULT_OK);
            accept_sequence(&parser, &model, 0, first_index);

            const size_t order[2] = {first_index + 1U, first_index};
            int rejected = 0;
            for (size_t swap_index = 0; swap_index < 2U; ++swap_index) {
                DBProtocolTransferMetadata swapped = metadata_for(
                    &model.roles[order[swap_index]]);
                DBTransitionResult result = db_transition_parser_accept(
                    &parser, &swapped);
                if (result == DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER) {
                    rejected = 1;
                    break;
                }
                assert(result == DB_TRANSITION_RESULT_OK);
            }
            for (size_t remainder = first_index + 2U;
                 !rejected && remainder < model.role_count; ++remainder) {
                DBProtocolTransferMetadata metadata = metadata_for(
                    &model.roles[remainder]);
                DBTransitionResult result = db_transition_parser_accept(
                    &parser, &metadata);
                if (result == DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER) {
                    rejected = 1;
                    break;
                }
                assert(result == DB_TRANSITION_RESULT_OK);
            }
            assert(rejected);
            assert(parser.state == DB_TRANSITION_STATE_FAILED);
            assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
        }
    }
}

static void
test_invalid_and_unexpected(void)
{
    Fixture fixture;
    fixture_initialize(&fixture);
    DBQualifiedSequence plug = sequence(
        DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX);
    DBProtocolTransferMetadata first = metadata_for(&plug.roles[0]);

    DBTransitionParser parser = {0};
    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    assert(db_transition_parser_accept(&parser, NULL) ==
        DB_TRANSITION_RESULT_INVALID_ARGUMENT);
    assert(parser.state == DB_TRANSITION_STATE_FAILED);

    DBProtocolTransferMetadata invalids[8];
    for (size_t index = 0; index < 8U; ++index) {
        invalids[index] = first;
    }
    invalids[0].endpoint = 0x85;
    invalids[1].succeeded = 0U;
    invalids[2].kind = DB_PROTOCOL_TRANSFER_KIND_INVALID;
    invalids[3].length = 1025U;
    invalids[3].in_prefix[2] = 0xfdU;
    invalids[3].in_prefix[3] = 0x03U;
    invalids[4].in_prefix[0] = 1U;
    invalids[5].length = 3U;
    invalids[6] = (DBProtocolTransferMetadata) {
        .direction = DB_PROTOCOL_DIRECTION_OUT,
        .endpoint = DB_MACHINE_ENDPOINT_OUT,
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,
        .succeeded = 1U,
        .length = 17U
    };
    invalids[7].direction = DB_PROTOCOL_DIRECTION_INVALID;
    for (size_t index = 0; index < 8U; ++index) {
        assert(db_transition_parser_initialize(&parser,
            DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX,
            &fixture.machine) == DB_TRANSITION_RESULT_OK);
        assert(db_transition_parser_accept(&parser, &invalids[index]) ==
            DB_TRANSITION_RESULT_INVALID_TRANSFER);
    }

    /* A structurally valid duplicate and an unauthorized swap fail closed. */
    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    assert(db_transition_parser_accept(&parser, &first) ==
        DB_TRANSITION_RESULT_OK);
    assert(db_transition_parser_accept(&parser, &first) ==
        DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER);

    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    DBProtocolTransferMetadata second = metadata_for(&plug.roles[1]);
    assert(db_transition_parser_accept(&parser, &second) ==
        DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER);

    assert(db_transition_parser_initialize(&parser,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    accept_sequence(&parser, &plug, 0, 7);
    assert(db_transition_parser_finish(&parser) ==
        DB_TRANSITION_RESULT_INCOMPLETE);
    assert(parser.state == DB_TRANSITION_STATE_FAILED);
    assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
}

static void
test_closed_profile_union(void)
{
    DBQualifiedSequence profile_a = sequence(
        DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_A);
    DBQualifiedSequence profile_b = sequence(
        DB_QUALIFIED_SEQUENCE_HOTUNPLUG_PROFILE_B);
    const size_t correlated_roles[] = {5U, 6U, 15U, 18U, 21U, 24U};
    for (uint32_t choices = 0U; choices < UINT32_C(32); ++choices) {
        Fixture fixture;
        fixture_initialize(&fixture);
        DBTransitionParser parser = {0};
        assert(db_transition_parser_initialize(&parser,
            DB_TRANSITION_KIND_HOTUNPLUG_CORRELATED_PROFILE,
            &fixture.machine) == DB_TRANSITION_RESULT_OK);
        int rejected = 0;
        for (size_t role_index = 0; role_index < profile_a.role_count;
             ++role_index) {
            uint32_t choice_bit = 0U;
            if (role_index == correlated_roles[0] ||
                role_index == correlated_roles[1]) {
                choice_bit = choices & UINT32_C(1);
            } else {
                for (size_t difference = 2U;
                     difference < sizeof(correlated_roles) /
                         sizeof(correlated_roles[0]); ++difference) {
                    if (role_index == correlated_roles[difference]) {
                        choice_bit = (choices >> (difference - 1U)) &
                            UINT32_C(1);
                    }
                }
            }
            const DBPartialOrderRole *role = choice_bit != 0U ?
                &profile_b.roles[role_index] : &profile_a.roles[role_index];
            DBProtocolTransferMetadata metadata = metadata_for(role);
            DBTransitionResult result = db_transition_parser_accept(&parser,
                &metadata);
            if (result == DB_TRANSITION_RESULT_UNEXPECTED_TRANSFER) {
                rejected = 1;
                break;
            }
            assert(result == (role_index + 1U == profile_a.role_count ?
                DB_TRANSITION_RESULT_COMPLETE : DB_TRANSITION_RESULT_OK));
        }
        if (choices == 0U || choices == UINT32_C(31)) {
            assert(!rejected);
            assert(parser.state == DB_TRANSITION_STATE_COMPLETE);
        } else {
            assert(rejected);
            assert(parser.state == DB_TRANSITION_STATE_FAILED);
        }
        assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
    }
}

static void
test_corruption_guards(void)
{
    Fixture fixture;
    fixture_initialize(&fixture);
    DBTransitionParser base = {0};
    assert(db_transition_parser_initialize(&base,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    DBQualifiedSequence plug = sequence(
        DB_QUALIFIED_SEQUENCE_HOTPLUG_PREFIX);
    DBProtocolTransferMetadata first = metadata_for(&plug.roles[0]);

    DBTransitionParser corrupt = base;
    corrupt.kind = (DBTransitionKind)99;
    assert(db_transition_parser_accept(&corrupt, &first) ==
        DB_TRANSITION_RESULT_CORRUPT_STATE);
    corrupt = base;
    corrupt.state = (DBTransitionState)99;
    assert(db_transition_parser_accept(&corrupt, &first) ==
        DB_TRANSITION_RESULT_CORRUPT_STATE);
    corrupt = base;
    corrupt.accepted_count = 99U;
    assert(db_transition_parser_accept(&corrupt, &first) ==
        DB_TRANSITION_RESULT_CORRUPT_STATE);
    corrupt = base;
    corrupt.active_profile_mask = 0U;
    assert(db_transition_parser_accept(&corrupt, &first) ==
        DB_TRANSITION_RESULT_CORRUPT_STATE);
    corrupt = base;
    corrupt.transport_lifecycle_epoch = 0U;
    assert(db_transition_parser_accept(&corrupt, &first) ==
        DB_TRANSITION_RESULT_CORRUPT_STATE);
    corrupt = base;
    corrupt.candidates[0].consumed_transfer_count = 1U;
    assert(db_transition_parser_accept(&corrupt, &first) ==
        DB_TRANSITION_RESULT_CORRUPT_STATE);

    DBTransitionParser completed = {0};
    assert(db_transition_parser_initialize(&completed,
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX, &fixture.machine) ==
        DB_TRANSITION_RESULT_OK);
    accept_sequence(&completed, &plug, 0, plug.role_count);
    completed.candidates[0].candidate_masks[0] = 0U;
    assert(db_transition_parser_observed_profile(&completed) ==
        DB_TRANSITION_PROFILE_NONE);
    assert(db_transition_parser_stage(&completed) ==
        DB_TRANSITION_STAGE_NOT_APPLICABLE);
    assert(db_transition_parser_finish(&completed) ==
        DB_TRANSITION_RESULT_CORRUPT_STATE);
    assert(completed.state == DB_TRANSITION_STATE_FAILED);
    assert(db_machine_detach(&fixture.machine) == DB_MACHINE_OK);
}

int
main(void)
{
    test_complete_profiles();
    test_binding();
    test_binding_loss_at_every_prefix();
    test_each_role_rejects_a_valid_wrong_length();
    test_every_observable_adjacent_swap_is_rejected();
    test_invalid_and_unexpected();
    test_closed_profile_union();
    test_corruption_guards();

    assert(strcmp(db_transition_kind_name(
        DB_TRANSITION_KIND_HOTPLUG_CORRELATED_PREFIX),
        "hotplug-correlated-prefix") == 0);
    assert(strcmp(db_transition_state_name(DB_TRANSITION_STATE_COMPLETE),
        "complete") == 0);
    assert(strcmp(db_transition_result_name(
        DB_TRANSITION_RESULT_BINDING_LOST), "binding-lost") == 0);
    assert(strcmp(db_transition_profile_name(
        DB_TRANSITION_PROFILE_HOTUNPLUG_A),
        "hotunplug-correlated-profile-a") == 0);
    assert(strcmp(db_transition_stage_name(
        DB_TRANSITION_STAGE_FIRST_15_OBSERVED),
        "first-15-observed") == 0);
    assert(strcmp(db_transition_result_name((DBTransitionResult)99),
        "invalid-result") == 0);
    return 0;
}
