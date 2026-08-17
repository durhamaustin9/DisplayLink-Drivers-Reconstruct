#ifndef DOCKBRIDGE_ACTIVATION_MODEL_H
#define DOCKBRIDGE_ACTIVATION_MODEL_H

#include "observation_model.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    DB_ACTIVATION_MAX_LINE = 1024,
    DB_ACTIVATION_MAX_EVENTS = 4096,
    DB_ACTIVATION_MAX_TRANSFER_LENGTH = 16 * 1024 * 1024,
    DB_ACTIVATION_MAX_TOTAL_BYTES = 80 * 1024 * 1024
};

typedef enum {
    DB_ACTIVATION_ACTION_UNSET = 0,
    DB_ACTIVATION_ACTION_COLD_CONNECT,
    DB_ACTIVATION_ACTION_WARM_START
} DBActivationAction;

typedef enum {
    DB_ACTIVATION_EVENT_MARKER = 1,
    DB_ACTIVATION_EVENT_TRANSFER
} DBActivationEventType;

typedef enum {
    DB_ACTIVATION_MARKER_CAPTURE_START = 1,
    DB_ACTIVATION_MARKER_ACTION_ISSUED,
    DB_ACTIVATION_MARKER_OUTPUT_STABLE,
    DB_ACTIVATION_MARKER_CAPTURE_END
} DBActivationMarker;

typedef enum {
    DB_ACTIVATION_DIRECTION_OUT = 1,
    DB_ACTIVATION_DIRECTION_IN
} DBActivationDirection;

typedef enum {
    DB_ACTIVATION_TRANSFER_CONTROL = 1,
    DB_ACTIVATION_TRANSFER_BULK
} DBActivationTransferType;

typedef struct {
    DBActivationDirection direction;
    DBActivationTransferType type;
    uint8_t endpoint;
    uint32_t length;
} DBActivationTransfer;

typedef struct {
    uint32_t sequence;
    uint64_t timestamp_us;
    DBActivationEventType type;
    union {
        DBActivationMarker marker;
        DBActivationTransfer transfer;
    } value;
} DBActivationEvent;

typedef struct {
    DBObservationOrigin origin;
    DBActivationAction action;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t revision;
    size_t event_count;
    size_t transfer_count;
    size_t activation_transfer_count;
    size_t activation_total_bytes;
    size_t action_event_index;
    size_t stable_event_index;
    DBActivationEvent events[DB_ACTIVATION_MAX_EVENTS];
} DBActivationEnvelope;

typedef enum {
    DB_ACTIVATION_OK = 0,
    DB_ACTIVATION_IO_ERROR,
    DB_ACTIVATION_FORMAT_ERROR,
    DB_ACTIVATION_BOUNDS_ERROR
} DBActivationResult;

DBActivationResult db_activation_parse(FILE *input,
    DBActivationEnvelope *envelope, char *error, size_t error_capacity);
const char *db_activation_action_name(DBActivationAction action);
const char *db_activation_marker_name(DBActivationMarker marker);
const char *db_activation_result_name(DBActivationResult result);

#endif
