#include "observation_model.h"

#include <stdio.h>

int
main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s observation.txt\n", argv[0]);
        return 64;
    }

    FILE *input = fopen(argv[1], "r");
    if (input == NULL) {
        perror("transcript-check");
        return 66;
    }

    DBObservationSummary summary = {0};
    char error[256] = {0};
    DBObservationResult result = db_observation_parse(
        input, &summary, error, sizeof(error));
    (void)fclose(input);
    if (result != DB_OBSERVATION_OK) {
        fprintf(stderr, "transcript-check: %s\n", error);
        return 65;
    }

    printf("origin=%s revision=0x%04x interfaces=%zu transfers=%zu bytes=%zu "
        "candidate-display-interface=%s\n",
        db_observation_origin_name(summary.origin), summary.device_revision,
        summary.interface_count, summary.transfer_count,
        summary.total_transfer_bytes,
        summary.candidate_display_interface_seen ? "yes" : "no");
    return 0;
}
