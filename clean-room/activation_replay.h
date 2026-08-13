#ifndef DOCKBRIDGE_ACTIVATION_REPLAY_H
#define DOCKBRIDGE_ACTIVATION_REPLAY_H

#include "activation_model.h"

#include <stddef.h>

typedef enum {
    DB_ACTIVATION_REPLAY_OK = 0,
    DB_ACTIVATION_REPLAY_INVALID_ARGUMENT,
    DB_ACTIVATION_REPLAY_TRANSPORT_ERROR,
    DB_ACTIVATION_REPLAY_INVARIANT_ERROR
} DBActivationReplayResult;

typedef struct {
    size_t bulk_transfers;
    size_t control_transfers_not_replayed;
    size_t zero_length_transfers;
    size_t synthetic_packets;
    size_t outbound_bytes;
    size_t inbound_bytes;
    size_t fake_write_attempts;
    size_t fake_write_successes;
    size_t fake_read_attempts;
    size_t fake_read_successes;
} DBActivationReplayReport;

DBActivationReplayResult db_activation_replay_fake(
    const DBActivationEnvelope *envelope, DBActivationReplayReport *report);
const char *db_activation_replay_result_name(DBActivationReplayResult result);

#endif
