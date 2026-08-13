#ifndef DOCKBRIDGE_OBSERVATION_MODEL_H
#define DOCKBRIDGE_OBSERVATION_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    DB_OBSERVATION_MAX_LINE = 1024,
    DB_OBSERVATION_MAX_INTERFACES = 256,
    DB_OBSERVATION_MAX_TRANSFERS = 65536,
    DB_OBSERVATION_MAX_TRANSFER_LENGTH = 16 * 1024 * 1024
};

typedef enum {
    DB_OBSERVATION_ORIGIN_UNSET = 0,
    DB_OBSERVATION_ORIGIN_SYNTHETIC,
    DB_OBSERVATION_ORIGIN_PUBLIC,
    DB_OBSERVATION_ORIGIN_BLACK_BOX
} DBObservationOrigin;

typedef enum {
    DB_OBSERVATION_OK = 0,
    DB_OBSERVATION_IO_ERROR,
    DB_OBSERVATION_FORMAT_ERROR,
    DB_OBSERVATION_BOUNDS_ERROR
} DBObservationResult;

typedef struct {
    DBObservationOrigin origin;
    uint32_t device_revision;
    size_t interface_count;
    size_t transfer_count;
    size_t total_transfer_bytes;
    int candidate_display_interface_seen;
} DBObservationSummary;

DBObservationResult db_observation_parse(FILE *input,
    DBObservationSummary *summary, char *error, size_t error_capacity);
const char *db_observation_origin_name(DBObservationOrigin origin);

#endif
