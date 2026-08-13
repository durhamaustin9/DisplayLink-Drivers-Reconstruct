#include "observation_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static DBObservationResult
parse_text(const char *text, DBObservationSummary *summary, char *error,
    size_t error_capacity)
{
    FILE *input = tmpfile();
    assert(input != NULL);
    assert(fputs(text, input) >= 0);
    rewind(input);
    DBObservationResult result = db_observation_parse(
        input, summary, error, error_capacity);
    assert(fclose(input) == 0);
    return result;
}

int
main(void)
{
    static const char valid[] =
        "dockbridge-observation-v1\n"
        "origin synthetic\n"
        "device 17e9 4323 3156\n"
        "interface 0 ff 00 03 0 2\n"
        "interface 2 01 01 20 0 1\n"
        "transfer 0 out control 00 64\n"
        "transfer 1 out bulk 02 4096\n"
        "transfer 2 in bulk 84 512\n";
    DBObservationSummary summary = {0};
    char error[256] = {0};

    assert(parse_text(valid, &summary, error, sizeof(error)) == DB_OBSERVATION_OK);
    assert(summary.origin == DB_OBSERVATION_ORIGIN_SYNTHETIC);
    assert(summary.device_revision == 0x3156);
    assert(summary.interface_count == 2);
    assert(summary.transfer_count == 3);
    assert(summary.total_transfer_bytes == 4672);
    assert(summary.candidate_display_interface_seen);

    static const char wrong_device[] =
        "dockbridge-observation-v1\norigin public\n"
        "device 17e9 6006 0001\ninterface 0 ff 00 03 0 2\n";
    assert(parse_text(wrong_device, &summary, error, sizeof(error)) ==
        DB_OBSERVATION_FORMAT_ERROR);

    static const char skipped_sequence[] =
        "dockbridge-observation-v1\norigin black-box\n"
        "device 17e9 4323 3156\ninterface 0 ff 00 03 0 2\n"
        "transfer 1 out bulk 02 128\n";
    assert(parse_text(skipped_sequence, &summary, error, sizeof(error)) ==
        DB_OBSERVATION_BOUNDS_ERROR);

    static const char oversized[] =
        "dockbridge-observation-v1\norigin synthetic\n"
        "device 17e9 4323 3156\ninterface 0 ff 00 03 0 2\n"
        "transfer 0 out bulk 02 16777217\n";
    assert(parse_text(oversized, &summary, error, sizeof(error)) ==
        DB_OBSERVATION_BOUNDS_ERROR);

    static const char payload_forbidden[] =
        "dockbridge-observation-v1\norigin synthetic\n"
        "device 17e9 4323 3156\ninterface 0 ff 00 03 0 2\n"
        "transfer 0 out bulk 02 4 deadbeef\n";
    assert(parse_text(payload_forbidden, &summary, error, sizeof(error)) ==
        DB_OBSERVATION_FORMAT_ERROR);

    static const char missing_origin[] =
        "dockbridge-observation-v1\n"
        "device 17e9 4323 3156\ninterface 0 ff 00 03 0 2\n";
    assert(parse_text(missing_origin, &summary, error, sizeof(error)) ==
        DB_OBSERVATION_FORMAT_ERROR);

    FILE *many_interfaces = tmpfile();
    assert(many_interfaces != NULL);
    assert(fputs("dockbridge-observation-v1\norigin synthetic\n"
        "device 17e9 4323 3156\n", many_interfaces) >= 0);
    for (size_t index = 0; index <= DB_OBSERVATION_MAX_INTERFACES; ++index) {
        assert(fputs("interface 0 ff 00 03 0 2\n", many_interfaces) >= 0);
    }
    rewind(many_interfaces);
    assert(db_observation_parse(many_interfaces, &summary, error, sizeof(error)) ==
        DB_OBSERVATION_BOUNDS_ERROR);
    assert(fclose(many_interfaces) == 0);

    assert(strcmp(db_observation_origin_name(DB_OBSERVATION_ORIGIN_PUBLIC),
        "public") == 0);
    assert(strcmp(db_observation_origin_name(DB_OBSERVATION_ORIGIN_UNSET),
        "unset") == 0);
    return 0;
}
