#include "observation_model.h"

#include "probe_model.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DB_MAX_TOKENS = 8 };

static void
set_error(char *error, size_t capacity, size_t line, const char *format, ...)
{
    if (error == NULL || capacity == 0) {
        return;
    }

    int prefix = snprintf(error, capacity, "line %zu: ", line);
    if (prefix < 0 || (size_t)prefix >= capacity) {
        error[capacity - 1] = '\0';
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(error + prefix, capacity - (size_t)prefix, format, arguments);
    va_end(arguments);
}

static int
parse_unsigned(const char *token, int base, uint32_t maximum, uint32_t *value)
{
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(token, &end, base);
    if (token[0] == '\0' || end == token || *end != '\0' || errno == ERANGE ||
        parsed > maximum) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static size_t
tokenize(char *line, char **tokens)
{
    size_t count = 0;
    char *state = NULL;
    char *token = strtok_r(line, " \t", &state);
    while (token != NULL && count < DB_MAX_TOKENS) {
        tokens[count++] = token;
        token = strtok_r(NULL, " \t", &state);
    }
    return token == NULL ? count : DB_MAX_TOKENS + 1U;
}

static DBObservationOrigin
parse_origin(const char *token)
{
    if (strcmp(token, "synthetic") == 0) {
        return DB_OBSERVATION_ORIGIN_SYNTHETIC;
    }
    if (strcmp(token, "public") == 0) {
        return DB_OBSERVATION_ORIGIN_PUBLIC;
    }
    if (strcmp(token, "black-box") == 0) {
        return DB_OBSERVATION_ORIGIN_BLACK_BOX;
    }
    return DB_OBSERVATION_ORIGIN_UNSET;
}

const char *
db_observation_origin_name(DBObservationOrigin origin)
{
    switch (origin) {
    case DB_OBSERVATION_ORIGIN_SYNTHETIC:
        return "synthetic";
    case DB_OBSERVATION_ORIGIN_PUBLIC:
        return "public";
    case DB_OBSERVATION_ORIGIN_BLACK_BOX:
        return "black-box";
    case DB_OBSERVATION_ORIGIN_UNSET:
        break;
    }
    return "unset";
}

DBObservationResult
db_observation_parse(FILE *input, DBObservationSummary *summary,
    char *error, size_t error_capacity)
{
    char line[DB_OBSERVATION_MAX_LINE + 2U];
    size_t line_number = 0;
    int header_seen = 0;
    int device_seen = 0;
    uint32_t expected_sequence = 0;
    DBObservationSummary parsed = {0};

    if (input == NULL || summary == NULL) {
        set_error(error, error_capacity, 0, "invalid parser argument");
        return DB_OBSERVATION_FORMAT_ERROR;
    }

    while (fgets(line, sizeof(line), input) != NULL) {
        ++line_number;
        size_t length = strlen(line);
        if (length == DB_OBSERVATION_MAX_LINE + 1U && line[length - 1] != '\n') {
            set_error(error, error_capacity, line_number, "line exceeds %u bytes",
                DB_OBSERVATION_MAX_LINE);
            return DB_OBSERVATION_BOUNDS_ERROR;
        }
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        char *start = line;
        while (*start == ' ' || *start == '\t') {
            ++start;
        }
        if (*start == '\0' || *start == '#') {
            continue;
        }

        char *tokens[DB_MAX_TOKENS] = {0};
        size_t count = tokenize(start, tokens);
        if (count > DB_MAX_TOKENS) {
            set_error(error, error_capacity, line_number, "too many fields");
            return DB_OBSERVATION_FORMAT_ERROR;
        }

        if (!header_seen) {
            if (count != 1 || strcmp(tokens[0], "dockbridge-observation-v1") != 0) {
                set_error(error, error_capacity, line_number,
                    "expected dockbridge-observation-v1 header");
                return DB_OBSERVATION_FORMAT_ERROR;
            }
            header_seen = 1;
            continue;
        }

        if (strcmp(tokens[0], "origin") == 0) {
            if (count != 2 || parsed.origin != DB_OBSERVATION_ORIGIN_UNSET ||
                (parsed.origin = parse_origin(tokens[1])) == DB_OBSERVATION_ORIGIN_UNSET) {
                set_error(error, error_capacity, line_number, "invalid or duplicate origin");
                return DB_OBSERVATION_FORMAT_ERROR;
            }
            continue;
        }

        if (strcmp(tokens[0], "device") == 0) {
            uint32_t vendor = 0;
            uint32_t product = 0;
            uint32_t revision = 0;
            if (count != 4 || device_seen ||
                !parse_unsigned(tokens[1], 16, UINT16_MAX, &vendor) ||
                !parse_unsigned(tokens[2], 16, UINT16_MAX, &product) ||
                !parse_unsigned(tokens[3], 16, UINT16_MAX, &revision) ||
                !db_probe_matches_device(vendor, product)) {
                set_error(error, error_capacity, line_number,
                    "invalid, duplicate, or non-allowlisted device");
                return DB_OBSERVATION_FORMAT_ERROR;
            }
            parsed.device_revision = revision;
            device_seen = 1;
            continue;
        }

        if (strcmp(tokens[0], "interface") == 0) {
            uint32_t values[6] = {0};
            if (count != 7 || !device_seen) {
                set_error(error, error_capacity, line_number,
                    "interface must follow the allowlisted device");
                return DB_OBSERVATION_FORMAT_ERROR;
            }
            if (parsed.interface_count >= DB_OBSERVATION_MAX_INTERFACES) {
                set_error(error, error_capacity, line_number,
                    "interface corpus exceeds configured bounds");
                return DB_OBSERVATION_BOUNDS_ERROR;
            }
            for (size_t index = 0; index < 6; ++index) {
                int base = index >= 1 && index <= 3 ? 16 : 10;
                if (!parse_unsigned(tokens[index + 1], base, UINT8_MAX,
                    &values[index])) {
                    set_error(error, error_capacity, line_number,
                        "invalid interface field");
                    return DB_OBSERVATION_BOUNDS_ERROR;
                }
            }
            DBProbeInterface descriptor = {
                values[0], values[1], values[2], values[3], values[4], values[5]
            };
            if (descriptor.number == 0 && descriptor.interface_class == 0xff &&
                descriptor.subclass == 0 && descriptor.protocol == 3) {
                parsed.candidate_display_interface_seen = 1;
            }
            ++parsed.interface_count;
            continue;
        }

        if (strcmp(tokens[0], "transfer") == 0) {
            uint32_t sequence = 0;
            uint32_t endpoint = 0;
            uint32_t transfer_length = 0;
            if (count != 6) {
                set_error(error, error_capacity, line_number,
                    "transfer metadata requires exactly five fields and no payload");
                return DB_OBSERVATION_FORMAT_ERROR;
            }
            if (!device_seen ||
                !parse_unsigned(tokens[1], 10, UINT32_MAX, &sequence) ||
                sequence != expected_sequence ||
                (strcmp(tokens[2], "in") != 0 && strcmp(tokens[2], "out") != 0) ||
                (strcmp(tokens[3], "control") != 0 && strcmp(tokens[3], "bulk") != 0 &&
                    strcmp(tokens[3], "interrupt") != 0 &&
                    strcmp(tokens[3], "isochronous") != 0) ||
                !parse_unsigned(tokens[4], 16, UINT8_MAX, &endpoint) ||
                (endpoint & 0x70U) != 0 ||
                !parse_unsigned(tokens[5], 10,
                    DB_OBSERVATION_MAX_TRANSFER_LENGTH, &transfer_length)) {
                set_error(error, error_capacity, line_number,
                    "invalid or out-of-sequence transfer metadata");
                return DB_OBSERVATION_BOUNDS_ERROR;
            }
            if (parsed.transfer_count >= DB_OBSERVATION_MAX_TRANSFERS ||
                parsed.total_transfer_bytes > SIZE_MAX - transfer_length) {
                set_error(error, error_capacity, line_number,
                    "transfer corpus exceeds configured bounds");
                return DB_OBSERVATION_BOUNDS_ERROR;
            }
            ++parsed.transfer_count;
            parsed.total_transfer_bytes += transfer_length;
            ++expected_sequence;
            continue;
        }

        set_error(error, error_capacity, line_number, "unknown record type");
        return DB_OBSERVATION_FORMAT_ERROR;
    }

    if (ferror(input)) {
        set_error(error, error_capacity, line_number, "input read failed");
        return DB_OBSERVATION_IO_ERROR;
    }
    if (!header_seen || parsed.origin == DB_OBSERVATION_ORIGIN_UNSET ||
        !device_seen || parsed.interface_count == 0) {
        set_error(error, error_capacity, line_number,
            "header, origin, device, and interface records are required");
        return DB_OBSERVATION_FORMAT_ERROR;
    }

    *summary = parsed;
    if (error != NULL && error_capacity > 0) {
        error[0] = '\0';
    }
    return DB_OBSERVATION_OK;
}
