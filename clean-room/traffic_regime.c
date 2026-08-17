#include "traffic_regime.h"

#include <stdint.h>

_Static_assert(DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH ==
    DB_PROTOCOL_MAX_TRANSFER_LENGTH,
    "traffic and protocol transfer bounds must remain equal");
_Static_assert(DB_PROTOCOL_IN_PREFIX_SIZE == 4,
    "the qualified inbound prefix is exactly four bytes");

static int
state_is_live(DBTrafficRegimeState state)
{
    switch (state) {
    case DB_TRAFFIC_REGIME_EMPTY:
    case DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED:
    case DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED:
    case DB_TRAFFIC_REGIME_OUT_65536_OBSERVED:
        return 1;
    case DB_TRAFFIC_REGIME_FAILED:
        return 0;
    }
    return 0;
}

static int
window_is_consistent(const DBTrafficRegimeWindow *window)
{
    if (window == NULL || !state_is_live(window->state) ||
        window->finished > 1U ||
        window->event_count > DB_TRAFFIC_REGIME_MAX_EVENTS ||
        window->out_event_count > window->event_count ||
        window->in_event_count !=
            window->event_count - window->out_event_count ||
        window->maximum_out_length >
            DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH ||
        (window->maximum_out_length != 0 &&
            window->maximum_out_length %
                DB_TRAFFIC_REGIME_OUT_ALIGNMENT != 0) ||
        window->maximum_in_length > DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH) {
        return 0;
    }

    if (window->event_count == 0) {
        return window->state == DB_TRAFFIC_REGIME_EMPTY &&
            window->out_event_count == 0 && window->in_event_count == 0 &&
            window->total_declared_bytes == 0 &&
            window->out_declared_bytes == 0 &&
            window->in_declared_bytes == 0 &&
            window->maximum_out_length == 0 &&
            window->maximum_in_length == 0;
    }
    if (window->state == DB_TRAFFIC_REGIME_EMPTY ||
        (window->out_event_count == 0) !=
            (window->maximum_out_length == 0) ||
        (window->in_event_count == 0) !=
            (window->maximum_in_length == 0) ||
        window->out_declared_bytes >
            UINT64_MAX - window->in_declared_bytes ||
        window->total_declared_bytes !=
            window->out_declared_bytes + window->in_declared_bytes) {
        return 0;
    }

    if (window->out_event_count > 0) {
        uint64_t minimum_out = (uint64_t)window->out_event_count *
                DB_TRAFFIC_REGIME_OUT_ALIGNMENT +
            (uint64_t)window->maximum_out_length -
                DB_TRAFFIC_REGIME_OUT_ALIGNMENT;
        uint64_t maximum_out = (uint64_t)window->out_event_count *
            (uint64_t)window->maximum_out_length;
        if (window->out_declared_bytes < minimum_out ||
            window->out_declared_bytes > maximum_out ||
            window->out_declared_bytes %
                DB_TRAFFIC_REGIME_OUT_ALIGNMENT != 0) {
            return 0;
        }
    } else if (window->out_declared_bytes != 0) {
        return 0;
    }
    if (window->in_event_count > 0) {
        uint64_t minimum_in = (uint64_t)window->in_event_count * 4U +
            (uint64_t)window->maximum_in_length - 4U;
        uint64_t maximum_in = (uint64_t)window->in_event_count *
            (uint64_t)window->maximum_in_length;
        if (window->in_declared_bytes < minimum_in ||
            window->in_declared_bytes > maximum_in) {
            return 0;
        }
    } else if (window->in_declared_bytes != 0) {
        return 0;
    }

    switch (window->state) {
    case DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED:
        return window->maximum_out_length <=
                DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH &&
            window->maximum_in_length <= DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH;
    case DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED:
        return window->maximum_out_length >
                DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH &&
            window->maximum_out_length <
                DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH;
    case DB_TRAFFIC_REGIME_OUT_65536_OBSERVED:
        return window->maximum_out_length ==
            DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH;
    case DB_TRAFFIC_REGIME_EMPTY:
    case DB_TRAFFIC_REGIME_FAILED:
        return 0;
    }
    return 0;
}

static DBTrafficRegimeResult
fail_window(DBTrafficRegimeWindow *window, DBTrafficRegimeResult result)
{
    window->state = DB_TRAFFIC_REGIME_FAILED;
    return result;
}

static int
metadata_is_qualified(const DBProtocolTransferMetadata *metadata)
{
    if (metadata == NULL ||
        metadata->kind != DB_PROTOCOL_TRANSFER_KIND_BULK ||
        metadata->succeeded != 1U || metadata->length == 0 ||
        metadata->length > DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH) {
        return 0;
    }

    if (metadata->direction == DB_PROTOCOL_DIRECTION_OUT) {
        if (metadata->endpoint != DB_TRAFFIC_REGIME_ENDPOINT_OUT ||
            metadata->length % DB_TRAFFIC_REGIME_OUT_ALIGNMENT != 0) {
            return 0;
        }
        for (size_t index = 0; index < sizeof(metadata->in_prefix); ++index) {
            if (metadata->in_prefix[index] != 0) {
                return 0;
            }
        }
        return 1;
    }

    if (metadata->direction != DB_PROTOCOL_DIRECTION_IN ||
        metadata->endpoint != DB_TRAFFIC_REGIME_ENDPOINT_IN ||
        metadata->length < sizeof(metadata->in_prefix) ||
        metadata->length > DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH ||
        metadata->in_prefix[0] != 0 || metadata->in_prefix[1] != 0) {
        return 0;
    }
    size_t declared_body_length = (size_t)metadata->in_prefix[2] |
        ((size_t)metadata->in_prefix[3] << 8U);
    return declared_body_length ==
        metadata->length - sizeof(metadata->in_prefix);
}

void
db_traffic_regime_initialize(DBTrafficRegimeWindow *window)
{
    if (window != NULL) {
        *window = (DBTrafficRegimeWindow) {
            .state = DB_TRAFFIC_REGIME_EMPTY
        };
    }
}

DBTrafficRegimeResult
db_traffic_regime_accept(DBTrafficRegimeWindow *window,
    const DBProtocolTransferMetadata *metadata)
{
    if (window == NULL) {
        return DB_TRAFFIC_REGIME_INVALID_ARGUMENT;
    }
    if (window->state == DB_TRAFFIC_REGIME_FAILED) {
        return DB_TRAFFIC_REGIME_STICKY_FAILURE;
    }
    if (!window_is_consistent(window)) {
        return fail_window(window, DB_TRAFFIC_REGIME_CORRUPTED);
    }
    if (window->finished != 0U) {
        return DB_TRAFFIC_REGIME_ALREADY_FINISHED;
    }
    if (!metadata_is_qualified(metadata)) {
        return fail_window(window, metadata == NULL ?
            DB_TRAFFIC_REGIME_INVALID_ARGUMENT :
            DB_TRAFFIC_REGIME_INVALID_METADATA);
    }
    if (window->event_count >= DB_TRAFFIC_REGIME_MAX_EVENTS ||
        window->total_declared_bytes >
            UINT64_MAX - (uint64_t)metadata->length ||
        (metadata->direction == DB_PROTOCOL_DIRECTION_OUT &&
            window->out_declared_bytes >
                UINT64_MAX - (uint64_t)metadata->length) ||
        (metadata->direction == DB_PROTOCOL_DIRECTION_IN &&
            window->in_declared_bytes >
                UINT64_MAX - (uint64_t)metadata->length)) {
        return fail_window(window, DB_TRAFFIC_REGIME_BOUNDS_EXCEEDED);
    }

    ++window->event_count;
    window->total_declared_bytes += (uint64_t)metadata->length;
    if (metadata->direction == DB_PROTOCOL_DIRECTION_OUT) {
        ++window->out_event_count;
        window->out_declared_bytes += (uint64_t)metadata->length;
        if (metadata->length > window->maximum_out_length) {
            window->maximum_out_length = metadata->length;
        }
        if (metadata->length == DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH) {
            window->state = DB_TRAFFIC_REGIME_OUT_65536_OBSERVED;
        } else if (metadata->length > DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH &&
            window->state != DB_TRAFFIC_REGIME_OUT_65536_OBSERVED) {
            window->state = DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED;
        } else if (window->state == DB_TRAFFIC_REGIME_EMPTY) {
            window->state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED;
        }
    } else {
        ++window->in_event_count;
        window->in_declared_bytes += (uint64_t)metadata->length;
        if (metadata->length > window->maximum_in_length) {
            window->maximum_in_length = metadata->length;
        }
        if (window->state == DB_TRAFFIC_REGIME_EMPTY) {
            window->state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED;
        }
    }
    return DB_TRAFFIC_REGIME_OK;
}

DBTrafficRegimeResult
db_traffic_regime_finish(DBTrafficRegimeWindow *window,
    DBTrafficRegimeState *observed_state)
{
    if (window == NULL || observed_state == NULL) {
        return DB_TRAFFIC_REGIME_INVALID_ARGUMENT;
    }
    if (window->state == DB_TRAFFIC_REGIME_FAILED) {
        return DB_TRAFFIC_REGIME_STICKY_FAILURE;
    }
    if (!window_is_consistent(window)) {
        return fail_window(window, DB_TRAFFIC_REGIME_CORRUPTED);
    }
    if (window->finished != 0U) {
        return DB_TRAFFIC_REGIME_ALREADY_FINISHED;
    }
    window->finished = 1U;
    *observed_state = window->state;
    return DB_TRAFFIC_REGIME_OK;
}

const char *
db_traffic_regime_state_name(DBTrafficRegimeState state)
{
    switch (state) {
    case DB_TRAFFIC_REGIME_EMPTY:
        return "empty";
    case DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED:
        return "small-only-observed";
    case DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED:
        return "out-above-1024-observed";
    case DB_TRAFFIC_REGIME_OUT_65536_OBSERVED:
        return "out-65536-observed";
    case DB_TRAFFIC_REGIME_FAILED:
        return "failed";
    }
    return "invalid-state";
}

const char *
db_traffic_regime_result_name(DBTrafficRegimeResult result)
{
    switch (result) {
    case DB_TRAFFIC_REGIME_OK:
        return "ok";
    case DB_TRAFFIC_REGIME_INVALID_ARGUMENT:
        return "invalid-argument";
    case DB_TRAFFIC_REGIME_INVALID_METADATA:
        return "invalid-metadata";
    case DB_TRAFFIC_REGIME_BOUNDS_EXCEEDED:
        return "bounds-exceeded";
    case DB_TRAFFIC_REGIME_CORRUPTED:
        return "corrupted";
    case DB_TRAFFIC_REGIME_ALREADY_FINISHED:
        return "already-finished";
    case DB_TRAFFIC_REGIME_STICKY_FAILURE:
        return "sticky-failure";
    }
    return "invalid-result";
}
