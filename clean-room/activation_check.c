#include "activation_model.h"
#include "activation_replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(const char *program)
{
    fprintf(stderr, "usage: %s [--replay-fake] activation-envelope\n", program);
}

int
main(int argc, char **argv)
{
    int replay_fake = 0;
    const char *path = NULL;
    if (argc == 2) {
        path = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--replay-fake") == 0) {
        replay_fake = 1;
        path = argv[2];
    } else {
        usage(argv[0]);
        return 64;
    }

    FILE *input = fopen(path, "r");
    if (input == NULL) {
        perror("activation-check");
        return 66;
    }
    DBActivationEnvelope *envelope = calloc(1, sizeof(*envelope));
    if (envelope == NULL) {
        fputs("activation-check: allocation failed\n", stderr);
        (void)fclose(input);
        return 71;
    }
    char error[256] = {0};
    DBActivationResult result = db_activation_parse(
        input, envelope, error, sizeof(error));
    (void)fclose(input);
    if (result != DB_ACTIVATION_OK) {
        fprintf(stderr, "activation-check: %s\n", error);
        free(envelope);
        return 65;
    }

    printf("origin=%s action=%s device=%04x:%04x revision=%04x events=%zu "
        "transfers=%zu activation-transfers=%zu activation-bytes=%zu\n",
        db_observation_origin_name(envelope->origin),
        db_activation_action_name(envelope->action), envelope->vendor_id,
        envelope->product_id, envelope->revision, envelope->event_count,
        envelope->transfer_count, envelope->activation_transfer_count,
        envelope->activation_total_bytes);
    puts("payloads=absent semantics=unknown hardware-writes=disabled");

    if (replay_fake) {
        DBActivationReplayReport report = {0};
        DBActivationReplayResult replay = db_activation_replay_fake(
            envelope, &report);
        if (replay != DB_ACTIVATION_REPLAY_OK) {
            fprintf(stderr, "activation-check: fake replay failed: %s\n",
                db_activation_replay_result_name(replay));
            free(envelope);
            return 70;
        }
        printf("fake-replay bulk-transfers=%zu control-not-replayed=%zu "
            "synthetic-packets=%zu out-bytes=%zu in-bytes=%zu "
            "fake-writes=%zu fake-reads=%zu\n",
            report.bulk_transfers, report.control_transfers_not_replayed,
            report.synthetic_packets, report.outbound_bytes,
            report.inbound_bytes, report.fake_write_successes,
            report.fake_read_successes);
    }

    free(envelope);
    return 0;
}
