#include "activation_model.h"
#include "activation_replay.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char malformed_envelope[] =
    "dockbridge-activation-envelope-v1\n"
    "origin black-box\n"
    "device 17e9 4323 3156\n"
    "action warm-start\n"
    "event 0 0 marker capture-start\n"
    "event 1 100 out-of-window-placeholder\n";

static const char complete_envelope[] =
    "dockbridge-activation-envelope-v1\n"
    "origin black-box\n"
    "device 17e9 4323 3156\n"
    "action warm-start\n"
    "event 0 0 marker capture-start\n"
    "event 1 100 transfer out control 00 8\n"
    "event 2 200 marker action-issued\n"
    "event 3 210 transfer out control 00 64\n"
    "event 4 220 transfer out bulk 02 2048\n"
    "event 5 230 transfer in bulk 84 1500\n"
    "event 6 240 transfer in bulk 84 0\n"
    "event 7 300 marker output-stable\n"
    "event 8 400 transfer in bulk 84 1024\n"
    "event 9 500 marker capture-end\n";

static DBActivationResult
parse_text(const char *text, DBActivationEnvelope *envelope,
    char *error, size_t error_capacity)
{
    FILE *file = tmpfile();
    assert(file != NULL);
    size_t length = strlen(text);
    assert(fwrite(text, 1, length, file) == length);
    rewind(file);
    DBActivationResult result = db_activation_parse(
        file, envelope, error, error_capacity);
    assert(fclose(file) == 0);
    return result;
}

static void
assert_rejected(const char *text)
{
    DBActivationEnvelope *envelope = calloc(1, sizeof(*envelope));
    assert(envelope != NULL);
    char error[256] = {0};
    assert(parse_text(text, envelope, error, sizeof(error)) != DB_ACTIVATION_OK);
    assert(error[0] != '\0');
    free(envelope);
}

static void
assert_activation_total_bound(void)
{
    FILE *file = tmpfile();
    assert(file != NULL);
    assert(fputs(
        "dockbridge-activation-envelope-v1\n"
        "origin synthetic\n"
        "device 17e9 4323 3156\n"
        "action warm-start\n"
        "event 0 0 marker capture-start\n"
        "event 1 1 marker action-issued\n",
        file) >= 0);
    for (unsigned sequence = 2; sequence < 8; ++sequence) {
        assert(fprintf(file, "event %u %u transfer out bulk 02 16777216\n",
            sequence, sequence) > 0);
    }
    assert(fputs(
        "event 8 8 marker output-stable\n"
        "event 9 9 marker capture-end\n",
        file) >= 0);
    rewind(file);
    DBActivationEnvelope *envelope = calloc(1, sizeof(*envelope));
    assert(envelope != NULL);
    char error[256] = {0};
    assert(db_activation_parse(file, envelope, error, sizeof(error)) ==
        DB_ACTIVATION_BOUNDS_ERROR);
    assert(error[0] != '\0');
    free(envelope);
    assert(fclose(file) == 0);
}

int
main(void)
{
    DBActivationEnvelope *envelope = calloc(1, sizeof(*envelope));
    assert(envelope != NULL);
    char error[256] = {0};
    assert(parse_text(complete_envelope, envelope, error, sizeof(error)) ==
        DB_ACTIVATION_OK);
    assert(error[0] == '\0');
    assert(envelope->origin == DB_OBSERVATION_ORIGIN_BLACK_BOX);
    assert(envelope->action == DB_ACTIVATION_ACTION_WARM_START);
    assert(envelope->vendor_id == 0x17e9);
    assert(envelope->product_id == 0x4323);
    assert(envelope->revision == 0x3156);
    assert(envelope->event_count == 10);
    assert(envelope->transfer_count == 6);
    assert(envelope->activation_transfer_count == 4);
    assert(envelope->activation_total_bytes == 3612);
    assert(envelope->action_event_index == 2);
    assert(envelope->stable_event_index == 7);

    DBActivationReplayReport report = {0};
    assert(db_activation_replay_fake(envelope, &report) ==
        DB_ACTIVATION_REPLAY_OK);
    assert(report.bulk_transfers == 3);
    assert(report.control_transfers_not_replayed == 1);
    assert(report.zero_length_transfers == 1);
    assert(report.synthetic_packets == 4);
    assert(report.outbound_bytes == 2048);
    assert(report.inbound_bytes == 1500);
    assert(report.fake_write_attempts == 2);
    assert(report.fake_write_successes == 2);
    assert(report.fake_read_attempts == 2);
    assert(report.fake_read_successes == 2);

    assert(strcmp(db_activation_action_name(DB_ACTIVATION_ACTION_WARM_START),
        "warm-start") == 0);
    assert(strcmp(db_activation_marker_name(
        DB_ACTIVATION_MARKER_OUTPUT_STABLE), "output-stable") == 0);
    assert(strcmp(db_activation_result_name(DB_ACTIVATION_BOUNDS_ERROR),
        "bounds-error") == 0);
    assert(strcmp(db_activation_replay_result_name(
        DB_ACTIVATION_REPLAY_INVARIANT_ERROR), "invariant-error") == 0);
    assert(db_activation_parse(NULL, envelope, error, sizeof(error)) ==
        DB_ACTIVATION_FORMAT_ERROR);
    assert(db_activation_replay_fake(NULL, &report) ==
        DB_ACTIVATION_REPLAY_INVALID_ARGUMENT);

    DBActivationEvent saved = envelope->events[3];
    envelope->events[3].type = DB_ACTIVATION_EVENT_MARKER;
    envelope->events[3].value.marker = DB_ACTIVATION_MARKER_CAPTURE_END;
    assert(db_activation_replay_fake(envelope, &report) ==
        DB_ACTIVATION_REPLAY_INVARIANT_ERROR);
    envelope->events[3] = saved;
    free(envelope);

    assert_rejected(malformed_envelope);
    assert_rejected(
        "dockbridge-activation-envelope-v1\n"
        "origin public\n"
        "device 17e9 4323 3156\n"
        "action warm-start\n");
    assert_rejected(
        "dockbridge-activation-envelope-v1\n"
        "origin synthetic\n"
        "device 17e9 4323 3157\n"
        "action warm-start\n");
    assert_rejected(
        "dockbridge-activation-envelope-v1\n"
        "origin synthetic\n"
        "device 17e9 4323 3156\n"
        "action warm-start\n"
        "event 0 0 marker capture-start\n"
        "event 1 1 marker action-issued\n"
        "event 2 2 transfer out bulk 03 1\n"
        "event 3 3 marker output-stable\n"
        "event 4 4 marker capture-end\n");
    assert_rejected(
        "dockbridge-activation-envelope-v1\n"
        "origin synthetic\n"
        "device 17e9 4323 3156\n"
        "action warm-start\n"
        "event 0 0 marker capture-start\n"
        "event 1 2 marker action-issued\n"
        "event 2 1 transfer in bulk 84 1\n"
        "event 3 3 marker output-stable\n"
        "event 4 4 marker capture-end\n");
    assert_rejected(
        "dockbridge-activation-envelope-v1\n"
        "origin synthetic\n"
        "device 17e9 4323 3156\n"
        "action cold-connect\n"
        "event 0 0 marker capture-start\n"
        "event 1 1 marker action-issued\n"
        "event 2 2 transfer in bulk 84 1 payload-forbidden\n"
        "event 3 3 marker output-stable\n"
        "event 4 4 marker capture-end\n");
    assert_rejected(
        "dockbridge-activation-envelope-v1\n"
        "origin synthetic\n"
        "device 17e9 4323 3156\n"
        "action cold-connect\n"
        "event 0 0 marker capture-start\n"
        "event 1 1 marker action-issued\n"
        "event 2 2 marker output-stable\n"
        "event 3 3 marker capture-end\n");
    assert_activation_total_bound();
    return 0;
}
