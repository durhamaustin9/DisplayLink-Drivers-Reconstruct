#ifndef DOCKBRIDGE_TRAFFIC_REGIME_H
#define DOCKBRIDGE_TRAFFIC_REGIME_H

#include "protocol_transfer.h"

#include <stddef.h>
#include <stdint.h>

enum {
    DB_TRAFFIC_REGIME_ENDPOINT_OUT = 0x02,
    DB_TRAFFIC_REGIME_ENDPOINT_IN = 0x84,
    DB_TRAFFIC_REGIME_OUT_ALIGNMENT = 16,
    DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH = 1024,
    DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH = 65536,
    DB_TRAFFIC_REGIME_MAX_EVENTS = 65536
};

/*
 * These names describe observations inside one explicitly finished window.
 * They do not identify a cable, monitor, video frame, activation state, or
 * transition caused by the absence of traffic.
 */
typedef enum {
    DB_TRAFFIC_REGIME_EMPTY = 0,
    DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED,
    DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED,
    DB_TRAFFIC_REGIME_OUT_65536_OBSERVED,
    DB_TRAFFIC_REGIME_FAILED
} DBTrafficRegimeState;

typedef enum {
    DB_TRAFFIC_REGIME_OK = 0,
    DB_TRAFFIC_REGIME_INVALID_ARGUMENT,
    DB_TRAFFIC_REGIME_INVALID_METADATA,
    DB_TRAFFIC_REGIME_BOUNDS_EXCEEDED,
    DB_TRAFFIC_REGIME_CORRUPTED,
    DB_TRAFFIC_REGIME_ALREADY_FINISHED,
    DB_TRAFFIC_REGIME_STICKY_FAILURE
} DBTrafficRegimeResult;

/*
 * This accumulator stores aggregate metadata only. It has no payload buffer,
 * heap allocation, clock, timeout, USB handle, or transport reference.
 */
typedef struct {
    DBTrafficRegimeState state;
    size_t event_count;
    size_t out_event_count;
    size_t in_event_count;
    uint64_t total_declared_bytes;
    uint64_t out_declared_bytes;
    uint64_t in_declared_bytes;
    size_t maximum_out_length;
    size_t maximum_in_length;
    uint8_t finished;
} DBTrafficRegimeWindow;

void db_traffic_regime_initialize(DBTrafficRegimeWindow *window);
DBTrafficRegimeResult db_traffic_regime_accept(DBTrafficRegimeWindow *window,
    const DBProtocolTransferMetadata *metadata);
DBTrafficRegimeResult db_traffic_regime_finish(DBTrafficRegimeWindow *window,
    DBTrafficRegimeState *observed_state);
const char *db_traffic_regime_state_name(DBTrafficRegimeState state);
const char *db_traffic_regime_result_name(DBTrafficRegimeResult result);

#endif
