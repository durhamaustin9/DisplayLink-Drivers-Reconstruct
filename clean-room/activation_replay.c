#include "activation_replay.h"

#include "fake_transport.h"

#include <string.h>

static DBActivationReplayResult
replay_chunk(DBTransport *transport, DBFakeTransport *fake,
    const DBActivationTransfer *transfer, const uint8_t *synthetic,
    size_t length)
{
    uint8_t received[DB_FAKE_MAX_PACKET_SIZE] = {0};
    size_t received_length = 0;
    DBTransportResult result;

    if (transfer->direction == DB_ACTIVATION_DIRECTION_OUT) {
        result = db_transport_write(transport, DB_FAKE_ENDPOINT_OUT,
            synthetic, length);
        if (result != DB_TRANSPORT_OK) {
            return DB_ACTIVATION_REPLAY_TRANSPORT_ERROR;
        }
        result = db_fake_transport_take_outbound(fake, received,
            sizeof(received), &received_length);
    } else {
        result = db_fake_transport_inject_inbound(fake, synthetic, length);
        if (result != DB_TRANSPORT_OK) {
            return DB_ACTIVATION_REPLAY_TRANSPORT_ERROR;
        }
        result = db_transport_read(transport, DB_FAKE_ENDPOINT_IN, received,
            sizeof(received), &received_length);
    }
    if (result != DB_TRANSPORT_OK) {
        return DB_ACTIVATION_REPLAY_TRANSPORT_ERROR;
    }
    if (received_length != length || memcmp(received, synthetic, length) != 0) {
        return DB_ACTIVATION_REPLAY_INVARIANT_ERROR;
    }
    return DB_ACTIVATION_REPLAY_OK;
}

DBActivationReplayResult
db_activation_replay_fake(const DBActivationEnvelope *envelope,
    DBActivationReplayReport *report)
{
    if (envelope == NULL || report == NULL || envelope->event_count == 0 ||
        envelope->action_event_index >= envelope->stable_event_index ||
        envelope->stable_event_index >= envelope->event_count) {
        return DB_ACTIVATION_REPLAY_INVALID_ARGUMENT;
    }

    DBFakeTransport fake = {0};
    DBTransport transport = {0};
    DBActivationReplayReport replayed = {0};
    uint8_t synthetic[DB_FAKE_MAX_PACKET_SIZE] = {0};
    db_fake_transport_initialize(&fake, &transport);
    if (db_transport_open(&transport) != DB_TRANSPORT_OK) {
        return DB_ACTIVATION_REPLAY_TRANSPORT_ERROR;
    }

    DBActivationReplayResult result = DB_ACTIVATION_REPLAY_OK;
    for (size_t index = envelope->action_event_index + 1U;
        index < envelope->stable_event_index; ++index) {
        const DBActivationEvent *event = &envelope->events[index];
        if (event->type != DB_ACTIVATION_EVENT_TRANSFER) {
            result = DB_ACTIVATION_REPLAY_INVARIANT_ERROR;
            break;
        }
        const DBActivationTransfer *transfer = &event->value.transfer;
        if (transfer->type == DB_ACTIVATION_TRANSFER_CONTROL) {
            ++replayed.control_transfers_not_replayed;
            continue;
        }
        if ((transfer->direction == DB_ACTIVATION_DIRECTION_OUT &&
                transfer->endpoint != DB_FAKE_ENDPOINT_OUT) ||
            (transfer->direction == DB_ACTIVATION_DIRECTION_IN &&
                transfer->endpoint != DB_FAKE_ENDPOINT_IN)) {
            result = DB_ACTIVATION_REPLAY_INVARIANT_ERROR;
            break;
        }

        ++replayed.bulk_transfers;
        if (transfer->length == 0) {
            ++replayed.zero_length_transfers;
            continue;
        }
        size_t remaining = transfer->length;
        while (remaining > 0) {
            size_t chunk = remaining < sizeof(synthetic) ?
                remaining : sizeof(synthetic);
            result = replay_chunk(&transport, &fake, transfer, synthetic, chunk);
            if (result != DB_ACTIVATION_REPLAY_OK) {
                break;
            }
            ++replayed.synthetic_packets;
            if (transfer->direction == DB_ACTIVATION_DIRECTION_OUT) {
                replayed.outbound_bytes += chunk;
            } else {
                replayed.inbound_bytes += chunk;
            }
            remaining -= chunk;
        }
        if (result != DB_ACTIVATION_REPLAY_OK) {
            break;
        }
    }

    replayed.fake_write_attempts = fake.write_attempt_count;
    replayed.fake_write_successes = fake.write_success_count;
    replayed.fake_read_attempts = fake.read_attempt_count;
    replayed.fake_read_successes = fake.read_success_count;
    db_transport_close(&transport);
    if (result == DB_ACTIVATION_REPLAY_OK) {
        *report = replayed;
    }
    return result;
}

const char *
db_activation_replay_result_name(DBActivationReplayResult result)
{
    switch (result) {
    case DB_ACTIVATION_REPLAY_OK:
        return "ok";
    case DB_ACTIVATION_REPLAY_INVALID_ARGUMENT:
        return "invalid-argument";
    case DB_ACTIVATION_REPLAY_TRANSPORT_ERROR:
        return "transport-error";
    case DB_ACTIVATION_REPLAY_INVARIANT_ERROR:
        return "invariant-error";
    }
    return "invalid-result";
}
