#include "activation_model.h"

#include "state_machine.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

enum { DB_ACTIVATION_MAX_TOKENS = 8 };

static void
set_error(char *error, size_t capacity, size_t line, const char *format, ...)
{
    if (error == NULL || capacity == 0) {
        return;
    }

    int prefix = snprintf(error, capacity, "line %zu: ", line);
    if (prefix < 0 || (size_t)prefix >= capacity) {
        error[capacity - 1U] = '\0';
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(error + prefix, capacity - (size_t)prefix, format,
        arguments);
    va_end(arguments);
}

static size_t
tokenize(char *line, char **tokens)
{
    size_t count = 0;
    char *state = NULL;
    char *token = strtok_r(line, " \t", &state);
    while (token != NULL && count < DB_ACTIVATION_MAX_TOKENS) {
        tokens[count++] = token;
        token = strtok_r(NULL, " \t", &state);
    }
    return token == NULL ? count : DB_ACTIVATION_MAX_TOKENS + 1U;
}

static int
parse_u64(const char *token, int base, uint64_t maximum, uint64_t *value)
{
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(token, &end, base);
    if (token[0] == '\0' || end == token || *end != '\0' || errno == ERANGE ||
        parsed > maximum) {
        return 0;
    }
    *value = (uint64_t)parsed;
    return 1;
}

static DBObservationOrigin
parse_origin(const char *token)
{
    if (strcmp(token, "synthetic") == 0) {
        return DB_OBSERVATION_ORIGIN_SYNTHETIC;
    }
    if (strcmp(token, "black-box") == 0) {
        return DB_OBSERVATION_ORIGIN_BLACK_BOX;
    }
    return DB_OBSERVATION_ORIGIN_UNSET;
}

static DBActivationAction
parse_action(const char *token)
{
    if (strcmp(token, "cold-connect") == 0) {
        return DB_ACTIVATION_ACTION_COLD_CONNECT;
    }
    if (strcmp(token, "warm-start") == 0) {
        return DB_ACTIVATION_ACTION_WARM_START;
    }
    return DB_ACTIVATION_ACTION_UNSET;
}

static DBActivationMarker
parse_marker(const char *token)
{
    if (strcmp(token, "capture-start") == 0) {
        return DB_ACTIVATION_MARKER_CAPTURE_START;
    }
    if (strcmp(token, "action-issued") == 0) {
        return DB_ACTIVATION_MARKER_ACTION_ISSUED;
    }
    if (strcmp(token, "output-stable") == 0) {
        return DB_ACTIVATION_MARKER_OUTPUT_STABLE;
    }
    if (strcmp(token, "capture-end") == 0) {
        return DB_ACTIVATION_MARKER_CAPTURE_END;
    }
    return 0;
}

static int
transfer_is_allowlisted(const DBActivationTransfer *transfer)
{
    if (transfer->type == DB_ACTIVATION_TRANSFER_CONTROL) {
        return transfer->endpoint == 0;
    }
    if (transfer->type != DB_ACTIVATION_TRANSFER_BULK) {
        return 0;
    }
    return (transfer->direction == DB_ACTIVATION_DIRECTION_OUT &&
            transfer->endpoint == DB_MACHINE_ENDPOINT_OUT) ||
        (transfer->direction == DB_ACTIVATION_DIRECTION_IN &&
            transfer->endpoint == DB_MACHINE_ENDPOINT_IN);
}

static int
advance_marker(DBActivationMarker marker, DBActivationMarker *expected)
{
    if (marker != *expected) {
        return 0;
    }
    if (*expected < DB_ACTIVATION_MARKER_CAPTURE_END) {
        *expected = (DBActivationMarker)(*expected + 1);
    } else {
        *expected = 0;
    }
    return 1;
}

DBActivationResult
db_activation_parse(FILE *input, DBActivationEnvelope *envelope,
    char *error, size_t error_capacity)
{
    char line[DB_ACTIVATION_MAX_LINE + 2U];
    size_t line_number = 0;
    int header_seen = 0;
    int device_seen = 0;
    int action_seen = 0;
    uint32_t expected_sequence = 0;
    uint64_t previous_timestamp = 0;
    int timestamp_seen = 0;
    DBActivationMarker expected_marker = DB_ACTIVATION_MARKER_CAPTURE_START;
    int activation_window = 0;
    DBActivationEnvelope parsed = {0};

    if (input == NULL || envelope == NULL) {
        set_error(error, error_capacity, 0, "invalid parser argument");
        return DB_ACTIVATION_FORMAT_ERROR;
    }

    while (fgets(line, sizeof(line), input) != NULL) {
        ++line_number;
        size_t length = strlen(line);
        if (length == DB_ACTIVATION_MAX_LINE + 1U && line[length - 1U] != '\n') {
            set_error(error, error_capacity, line_number,
                "line exceeds %u bytes", DB_ACTIVATION_MAX_LINE);
            return DB_ACTIVATION_BOUNDS_ERROR;
        }
        while (length > 0 &&
            (line[length - 1U] == '\n' || line[length - 1U] == '\r')) {
            line[--length] = '\0';
        }

        char *start = line;
        while (*start == ' ' || *start == '\t') {
            ++start;
        }
        if (*start == '\0' || *start == '#') {
            continue;
        }

        char *tokens[DB_ACTIVATION_MAX_TOKENS] = {0};
        size_t count = tokenize(start, tokens);
        if (count > DB_ACTIVATION_MAX_TOKENS) {
            set_error(error, error_capacity, line_number, "too many fields");
            return DB_ACTIVATION_FORMAT_ERROR;
        }

        if (!header_seen) {
            if (count != 1 ||
                strcmp(tokens[0], "dockbridge-activation-envelope-v1") != 0) {
                set_error(error, error_capacity, line_number,
                    "expected dockbridge-activation-envelope-v1 header");
                return DB_ACTIVATION_FORMAT_ERROR;
            }
            header_seen = 1;
            continue;
        }

        if (strcmp(tokens[0], "origin") == 0) {
            if (parsed.event_count != 0 || count != 2 ||
                parsed.origin != DB_OBSERVATION_ORIGIN_UNSET ||
                (parsed.origin = parse_origin(tokens[1])) ==
                    DB_OBSERVATION_ORIGIN_UNSET) {
                set_error(error, error_capacity, line_number,
                    "invalid or duplicate origin");
                return DB_ACTIVATION_FORMAT_ERROR;
            }
            continue;
        }

        if (strcmp(tokens[0], "device") == 0) {
            uint64_t vendor = 0;
            uint64_t product = 0;
            uint64_t revision = 0;
            if (parsed.event_count != 0 || count != 4 || device_seen ||
                !parse_u64(tokens[1], 16, UINT16_MAX, &vendor) ||
                !parse_u64(tokens[2], 16, UINT16_MAX, &product) ||
                !parse_u64(tokens[3], 16, UINT16_MAX, &revision) ||
                vendor != DB_MACHINE_VENDOR_ID ||
                product != DB_MACHINE_PRODUCT_ID ||
                revision != DB_MACHINE_DEVICE_REVISION) {
                set_error(error, error_capacity, line_number,
                    "device must exactly match 17e9:4323 revision 3156");
                return DB_ACTIVATION_FORMAT_ERROR;
            }
            parsed.vendor_id = (uint16_t)vendor;
            parsed.product_id = (uint16_t)product;
            parsed.revision = (uint16_t)revision;
            device_seen = 1;
            continue;
        }

        if (strcmp(tokens[0], "action") == 0) {
            if (parsed.event_count != 0 || count != 2 || action_seen ||
                (parsed.action = parse_action(tokens[1])) ==
                    DB_ACTIVATION_ACTION_UNSET) {
                set_error(error, error_capacity, line_number,
                    "invalid or duplicate activation action");
                return DB_ACTIVATION_FORMAT_ERROR;
            }
            action_seen = 1;
            continue;
        }

        if (strcmp(tokens[0], "event") != 0 ||
            parsed.origin == DB_OBSERVATION_ORIGIN_UNSET || !device_seen ||
            !action_seen) {
            set_error(error, error_capacity, line_number,
                "event requires origin, exact device, and action");
            return DB_ACTIVATION_FORMAT_ERROR;
        }
        if (parsed.event_count >= DB_ACTIVATION_MAX_EVENTS) {
            set_error(error, error_capacity, line_number,
                "activation envelope exceeds event bound");
            return DB_ACTIVATION_BOUNDS_ERROR;
        }

        uint64_t sequence = 0;
        uint64_t timestamp = 0;
        if (count < 4 || !parse_u64(tokens[1], 10, UINT32_MAX, &sequence) ||
            sequence != expected_sequence ||
            !parse_u64(tokens[2], 10, UINT64_MAX, &timestamp) ||
            (timestamp_seen && timestamp < previous_timestamp)) {
            set_error(error, error_capacity, line_number,
                "invalid sequence or non-monotonic timestamp");
            return DB_ACTIVATION_FORMAT_ERROR;
        }

        DBActivationEvent event = {
            .sequence = (uint32_t)sequence,
            .timestamp_us = timestamp
        };
        if (strcmp(tokens[3], "marker") == 0) {
            DBActivationMarker marker = count == 5 ? parse_marker(tokens[4]) : 0;
            if (marker == 0 || !advance_marker(marker, &expected_marker)) {
                set_error(error, error_capacity, line_number,
                    "marker is missing, duplicated, or out of order");
                return DB_ACTIVATION_FORMAT_ERROR;
            }
            event.type = DB_ACTIVATION_EVENT_MARKER;
            event.value.marker = marker;
            if (marker == DB_ACTIVATION_MARKER_ACTION_ISSUED) {
                parsed.action_event_index = parsed.event_count;
                activation_window = 1;
            } else if (marker == DB_ACTIVATION_MARKER_OUTPUT_STABLE) {
                parsed.stable_event_index = parsed.event_count;
                activation_window = 0;
            }
        } else if (strcmp(tokens[3], "transfer") == 0) {
            uint64_t endpoint = 0;
            uint64_t transfer_length = 0;
            if (count != 8 || expected_marker ==
                    DB_ACTIVATION_MARKER_CAPTURE_START || expected_marker == 0 ||
                (strcmp(tokens[4], "out") != 0 &&
                    strcmp(tokens[4], "in") != 0) ||
                (strcmp(tokens[5], "control") != 0 &&
                    strcmp(tokens[5], "bulk") != 0) ||
                !parse_u64(tokens[6], 16, UINT8_MAX, &endpoint) ||
                !parse_u64(tokens[7], 10, DB_ACTIVATION_MAX_TRANSFER_LENGTH,
                    &transfer_length)) {
                set_error(error, error_capacity, line_number,
                    "invalid transfer envelope");
                return DB_ACTIVATION_FORMAT_ERROR;
            }
            event.type = DB_ACTIVATION_EVENT_TRANSFER;
            event.value.transfer = (DBActivationTransfer) {
                .direction = strcmp(tokens[4], "out") == 0 ?
                    DB_ACTIVATION_DIRECTION_OUT : DB_ACTIVATION_DIRECTION_IN,
                .type = strcmp(tokens[5], "control") == 0 ?
                    DB_ACTIVATION_TRANSFER_CONTROL : DB_ACTIVATION_TRANSFER_BULK,
                .endpoint = (uint8_t)endpoint,
                .length = (uint32_t)transfer_length
            };
            if (!transfer_is_allowlisted(&event.value.transfer)) {
                set_error(error, error_capacity, line_number,
                    "transfer is outside endpoint/type allowlist");
                return DB_ACTIVATION_FORMAT_ERROR;
            }
            if (parsed.activation_total_bytes >
                DB_ACTIVATION_MAX_TOTAL_BYTES - (size_t)transfer_length) {
                set_error(error, error_capacity, line_number,
                    "activation byte total exceeds configured bound");
                return DB_ACTIVATION_BOUNDS_ERROR;
            }
            ++parsed.transfer_count;
            if (activation_window) {
                ++parsed.activation_transfer_count;
                parsed.activation_total_bytes += (size_t)transfer_length;
            }
        } else {
            set_error(error, error_capacity, line_number,
                "event must be marker or transfer metadata");
            return DB_ACTIVATION_FORMAT_ERROR;
        }

        parsed.events[parsed.event_count++] = event;
        ++expected_sequence;
        previous_timestamp = timestamp;
        timestamp_seen = 1;
    }

    if (ferror(input)) {
        set_error(error, error_capacity, line_number, "input read failed");
        return DB_ACTIVATION_IO_ERROR;
    }
    if (!header_seen || parsed.origin == DB_OBSERVATION_ORIGIN_UNSET ||
        !device_seen || !action_seen || expected_marker != 0 ||
        parsed.event_count < 4 ||
        parsed.events[0].type != DB_ACTIVATION_EVENT_MARKER ||
        parsed.events[0].value.marker != DB_ACTIVATION_MARKER_CAPTURE_START ||
        parsed.events[parsed.event_count - 1U].type !=
            DB_ACTIVATION_EVENT_MARKER ||
        parsed.events[parsed.event_count - 1U].value.marker !=
            DB_ACTIVATION_MARKER_CAPTURE_END ||
        parsed.activation_transfer_count == 0) {
        set_error(error, error_capacity, line_number,
            "complete marker sequence and at least one activation transfer are required");
        return DB_ACTIVATION_FORMAT_ERROR;
    }

    *envelope = parsed;
    if (error != NULL && error_capacity > 0) {
        error[0] = '\0';
    }
    return DB_ACTIVATION_OK;
}

const char *
db_activation_action_name(DBActivationAction action)
{
    switch (action) {
    case DB_ACTIVATION_ACTION_COLD_CONNECT:
        return "cold-connect";
    case DB_ACTIVATION_ACTION_WARM_START:
        return "warm-start";
    case DB_ACTIVATION_ACTION_UNSET:
        break;
    }
    return "unset";
}

const char *
db_activation_marker_name(DBActivationMarker marker)
{
    switch (marker) {
    case DB_ACTIVATION_MARKER_CAPTURE_START:
        return "capture-start";
    case DB_ACTIVATION_MARKER_ACTION_ISSUED:
        return "action-issued";
    case DB_ACTIVATION_MARKER_OUTPUT_STABLE:
        return "output-stable";
    case DB_ACTIVATION_MARKER_CAPTURE_END:
        return "capture-end";
    }
    return "invalid-marker";
}

const char *
db_activation_result_name(DBActivationResult result)
{
    switch (result) {
    case DB_ACTIVATION_OK:
        return "ok";
    case DB_ACTIVATION_IO_ERROR:
        return "io-error";
    case DB_ACTIVATION_FORMAT_ERROR:
        return "format-error";
    case DB_ACTIVATION_BOUNDS_ERROR:
        return "bounds-error";
    }
    return "invalid-result";
}
