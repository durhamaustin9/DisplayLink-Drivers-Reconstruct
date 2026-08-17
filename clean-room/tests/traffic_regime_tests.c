#include "traffic_regime.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static DBProtocolTransferMetadata
make_out(size_t length)
{
    return (DBProtocolTransferMetadata) {
        .direction = DB_PROTOCOL_DIRECTION_OUT,
        .endpoint = DB_TRAFFIC_REGIME_ENDPOINT_OUT,
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,
        .succeeded = 1U,
        .length = length
    };
}

static DBProtocolTransferMetadata
make_in(size_t length)
{
    assert(length >= 4U && length <= DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH);
    size_t body_length = length - 4U;
    return (DBProtocolTransferMetadata) {
        .direction = DB_PROTOCOL_DIRECTION_IN,
        .endpoint = DB_TRAFFIC_REGIME_ENDPOINT_IN,
        .kind = DB_PROTOCOL_TRANSFER_KIND_BULK,
        .succeeded = 1U,
        .length = length,
        .in_prefix = {
            0,
            0,
            (uint8_t)(body_length & 0xffU),
            (uint8_t)((body_length >> 8U) & 0xffU)
        }
    };
}

static void
assert_invalid_metadata(DBProtocolTransferMetadata metadata)
{
    DBTrafficRegimeWindow window = {0};
    db_traffic_regime_initialize(&window);
    assert(db_traffic_regime_accept(&window, &metadata) ==
        DB_TRAFFIC_REGIME_INVALID_METADATA);
    assert(window.state == DB_TRAFFIC_REGIME_FAILED);
    assert(window.event_count == 0);
    DBProtocolTransferMetadata valid = make_out(16);
    assert(db_traffic_regime_accept(&window, &valid) ==
        DB_TRAFFIC_REGIME_STICKY_FAILURE);
    DBTrafficRegimeState observed = DB_TRAFFIC_REGIME_EMPTY;
    assert(db_traffic_regime_finish(&window, &observed) ==
        DB_TRAFFIC_REGIME_STICKY_FAILURE);
}

static void
assert_corrupt_on_accept(DBTrafficRegimeWindow window)
{
    DBProtocolTransferMetadata metadata = make_out(16);
    assert(db_traffic_regime_accept(&window, &metadata) ==
        DB_TRAFFIC_REGIME_CORRUPTED);
    assert(window.state == DB_TRAFFIC_REGIME_FAILED);
    assert(db_traffic_regime_accept(&window, &metadata) ==
        DB_TRAFFIC_REGIME_STICKY_FAILURE);
}

int
main(void)
{
    db_traffic_regime_initialize(NULL);
    assert(db_traffic_regime_accept(NULL, NULL) ==
        DB_TRAFFIC_REGIME_INVALID_ARGUMENT);
    assert(db_traffic_regime_finish(NULL, NULL) ==
        DB_TRAFFIC_REGIME_INVALID_ARGUMENT);

    DBTrafficRegimeWindow window = {
        .state = DB_TRAFFIC_REGIME_FAILED,
        .event_count = 99
    };
    db_traffic_regime_initialize(&window);
    assert(window.state == DB_TRAFFIC_REGIME_EMPTY);
    assert(window.event_count == 0);
    assert(window.out_event_count == 0);
    assert(window.in_event_count == 0);
    assert(window.total_declared_bytes == 0);
    assert(window.out_declared_bytes == 0);
    assert(window.in_declared_bytes == 0);
    assert(window.maximum_out_length == 0);
    assert(window.maximum_in_length == 0);
    assert(window.finished == 0);

    /* A silent, explicitly closed window reports only that it was empty. */
    DBTrafficRegimeState observed = DB_TRAFFIC_REGIME_FAILED;
    assert(db_traffic_regime_finish(&window, &observed) ==
        DB_TRAFFIC_REGIME_OK);
    assert(observed == DB_TRAFFIC_REGIME_EMPTY);
    assert(window.state == DB_TRAFFIC_REGIME_EMPTY);
    assert(window.finished == 1);
    assert(db_traffic_regime_finish(&window, &observed) ==
        DB_TRAFFIC_REGIME_ALREADY_FINISHED);
    DBProtocolTransferMetadata small_out = make_out(16);
    DBTrafficRegimeWindow finished_copy = window;
    assert(db_traffic_regime_accept(&window, &small_out) ==
        DB_TRAFFIC_REGIME_ALREADY_FINISHED);
    assert(memcmp(&window, &finished_copy, sizeof(window)) == 0);

    db_traffic_regime_initialize(&window);
    DBProtocolTransferMetadata largest_small_out =
        make_out(DB_TRAFFIC_REGIME_SMALL_MAX_LENGTH);
    DBProtocolTransferMetadata smallest_in = make_in(4);
    DBProtocolTransferMetadata other_in = make_in(549);
    assert(db_traffic_regime_accept(&window, &largest_small_out) ==
        DB_TRAFFIC_REGIME_OK);
    assert(db_traffic_regime_accept(&window, &smallest_in) ==
        DB_TRAFFIC_REGIME_OK);
    assert(db_traffic_regime_accept(&window, &other_in) ==
        DB_TRAFFIC_REGIME_OK);
    assert(window.state == DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED);
    assert(window.event_count == 3);
    assert(window.out_event_count == 1);
    assert(window.in_event_count == 2);
    assert(window.total_declared_bytes == 1024U + 4U + 549U);
    assert(window.out_declared_bytes == 1024U);
    assert(window.in_declared_bytes == 4U + 549U);
    assert(window.maximum_out_length == 1024);
    assert(window.maximum_in_length == 549);
    observed = DB_TRAFFIC_REGIME_FAILED;
    assert(db_traffic_regime_finish(&window, &observed) ==
        DB_TRAFFIC_REGIME_OK);
    assert(observed == DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED);

    db_traffic_regime_initialize(&window);
    DBProtocolTransferMetadata above = make_out(1040);
    DBProtocolTransferMetadata below_maximum = make_out(65520);
    assert(db_traffic_regime_accept(&window, &above) ==
        DB_TRAFFIC_REGIME_OK);
    assert(window.state == DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED);
    assert(db_traffic_regime_accept(&window, &smallest_in) ==
        DB_TRAFFIC_REGIME_OK);
    assert(db_traffic_regime_accept(&window, &below_maximum) ==
        DB_TRAFFIC_REGIME_OK);
    assert(db_traffic_regime_accept(&window, &small_out) ==
        DB_TRAFFIC_REGIME_OK);
    assert(window.state == DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED);
    assert(window.maximum_out_length == 65520);
    assert(window.maximum_in_length == 4);

    DBProtocolTransferMetadata maximum =
        make_out(DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH);
    assert(db_traffic_regime_accept(&window, &maximum) ==
        DB_TRAFFIC_REGIME_OK);
    assert(window.state == DB_TRAFFIC_REGIME_OUT_65536_OBSERVED);
    assert(db_traffic_regime_accept(&window, &above) ==
        DB_TRAFFIC_REGIME_OK);
    assert(db_traffic_regime_accept(&window, &smallest_in) ==
        DB_TRAFFIC_REGIME_OK);
    assert(window.state == DB_TRAFFIC_REGIME_OUT_65536_OBSERVED);
    assert(window.maximum_out_length == 65536);
    assert(db_traffic_regime_finish(&window, &observed) ==
        DB_TRAFFIC_REGIME_OK);
    assert(observed == DB_TRAFFIC_REGIME_OUT_65536_OBSERVED);

    db_traffic_regime_initialize(&window);
    assert(db_traffic_regime_accept(&window, NULL) ==
        DB_TRAFFIC_REGIME_INVALID_ARGUMENT);
    assert(window.state == DB_TRAFFIC_REGIME_FAILED);
    assert(db_traffic_regime_accept(&window, &small_out) ==
        DB_TRAFFIC_REGIME_STICKY_FAILURE);

    DBProtocolTransferMetadata invalid = make_out(16);
    invalid.direction = DB_PROTOCOL_DIRECTION_INVALID;
    assert_invalid_metadata(invalid);
    invalid = make_out(16);
    invalid.direction = (DBProtocolDirection)99;
    assert_invalid_metadata(invalid);
    invalid = make_out(16);
    invalid.endpoint = DB_TRAFFIC_REGIME_ENDPOINT_IN;
    assert_invalid_metadata(invalid);
    invalid = make_in(16);
    invalid.endpoint = DB_TRAFFIC_REGIME_ENDPOINT_OUT;
    assert_invalid_metadata(invalid);
    invalid = make_out(16);
    invalid.kind = DB_PROTOCOL_TRANSFER_KIND_INVALID;
    assert_invalid_metadata(invalid);
    invalid = make_out(16);
    invalid.kind = (DBProtocolTransferKind)99;
    assert_invalid_metadata(invalid);
    invalid = make_out(16);
    invalid.succeeded = 0;
    assert_invalid_metadata(invalid);
    invalid = make_out(16);
    invalid.succeeded = 2;
    assert_invalid_metadata(invalid);
    invalid = make_out(0);
    assert_invalid_metadata(invalid);
    invalid = make_out(15);
    assert_invalid_metadata(invalid);
    invalid = make_out(17);
    assert_invalid_metadata(invalid);
    invalid = make_out(DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH + 1U);
    assert_invalid_metadata(invalid);
    invalid = make_out(16);
    invalid.in_prefix[3] = 1;
    assert_invalid_metadata(invalid);
    invalid = make_in(4);
    invalid.length = 3;
    assert_invalid_metadata(invalid);
    invalid = make_in(16);
    invalid.in_prefix[0] = 1;
    assert_invalid_metadata(invalid);
    invalid = make_in(16);
    invalid.in_prefix[1] = 1;
    assert_invalid_metadata(invalid);
    invalid = make_in(16);
    ++invalid.in_prefix[2];
    assert_invalid_metadata(invalid);
    invalid = make_in(1024);
    invalid.length = 1025;
    invalid.in_prefix[2] = 0xfd;
    invalid.in_prefix[3] = 0x03;
    assert_invalid_metadata(invalid);

    /* The 65,536th event is accepted; the next one fails closed. */
    db_traffic_regime_initialize(&window);
    DBProtocolTransferMetadata minimum_out =
        make_out(DB_TRAFFIC_REGIME_OUT_ALIGNMENT);
    for (size_t index = 0; index < DB_TRAFFIC_REGIME_MAX_EVENTS; ++index) {
        assert(db_traffic_regime_accept(&window, &minimum_out) ==
            DB_TRAFFIC_REGIME_OK);
    }
    assert(window.event_count == DB_TRAFFIC_REGIME_MAX_EVENTS);
    assert(window.total_declared_bytes ==
        (uint64_t)DB_TRAFFIC_REGIME_MAX_EVENTS *
            DB_TRAFFIC_REGIME_OUT_ALIGNMENT);
    assert(window.state == DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED);
    assert(db_traffic_regime_accept(&window, &minimum_out) ==
        DB_TRAFFIC_REGIME_BOUNDS_EXCEEDED);
    assert(window.state == DB_TRAFFIC_REGIME_FAILED);
    assert(db_traffic_regime_accept(&window, &minimum_out) ==
        DB_TRAFFIC_REGIME_STICKY_FAILURE);

    window = (DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_OUT_65536_OBSERVED,
        .event_count = DB_TRAFFIC_REGIME_MAX_EVENTS,
        .out_event_count = DB_TRAFFIC_REGIME_MAX_EVENTS,
        .total_declared_bytes =
            (uint64_t)DB_TRAFFIC_REGIME_MAX_EVENTS *
                DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH,
        .out_declared_bytes =
            (uint64_t)DB_TRAFFIC_REGIME_MAX_EVENTS *
                DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH,
        .maximum_out_length = DB_TRAFFIC_REGIME_MAX_DECLARED_LENGTH
    };
    assert(db_traffic_regime_accept(&window, &maximum) ==
        DB_TRAFFIC_REGIME_BOUNDS_EXCEEDED);
    assert(window.state == DB_TRAFFIC_REGIME_FAILED);

    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = (DBTrafficRegimeState)99
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_EMPTY,
        .finished = 2
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_EMPTY,
        .event_count = DB_TRAFFIC_REGIME_MAX_EVENTS + 1U
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED,
        .event_count = 1,
        .out_event_count = 2,
        .total_declared_bytes = 16,
        .maximum_out_length = 16
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED,
        .event_count = 2,
        .out_event_count = 1,
        .in_event_count = 0,
        .total_declared_bytes = 32,
        .maximum_out_length = 16
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_EMPTY,
        .event_count = 1,
        .out_event_count = 1,
        .total_declared_bytes = 16,
        .maximum_out_length = 16
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED,
        .event_count = 1,
        .out_event_count = 1,
        .total_declared_bytes = 0,
        .maximum_out_length = 16
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED,
        .event_count = 1,
        .out_event_count = 1,
        .total_declared_bytes = 1,
        .maximum_out_length = 1025
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED,
        .event_count = 1,
        .out_event_count = 1,
        .total_declared_bytes = 1024,
        .maximum_out_length = 1024
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_OUT_65536_OBSERVED,
        .event_count = 1,
        .out_event_count = 1,
        .total_declared_bytes = 65535,
        .maximum_out_length = 65535
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED,
        .event_count = 1,
        .in_event_count = 1,
        .total_declared_bytes = 1025,
        .maximum_in_length = 1025
    });
    assert_corrupt_on_accept((DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED,
        .event_count = 2,
        .out_event_count = 2,
        .total_declared_bytes = UINT64_MAX,
        .maximum_out_length = 16
    });

    window = (DBTrafficRegimeWindow) {
        .state = DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED,
        .event_count = 1,
        .out_event_count = 1,
        .total_declared_bytes = 16,
        .maximum_out_length = 16,
        .finished = 2
    };
    assert(db_traffic_regime_finish(&window, &observed) ==
        DB_TRAFFIC_REGIME_CORRUPTED);
    assert(window.state == DB_TRAFFIC_REGIME_FAILED);

    db_traffic_regime_initialize(&window);
    assert(db_traffic_regime_finish(&window, NULL) ==
        DB_TRAFFIC_REGIME_INVALID_ARGUMENT);
    assert(window.state == DB_TRAFFIC_REGIME_EMPTY);
    assert(window.finished == 0);

    assert(strcmp(db_traffic_regime_state_name(DB_TRAFFIC_REGIME_EMPTY),
        "empty") == 0);
    assert(strcmp(db_traffic_regime_state_name(
        DB_TRAFFIC_REGIME_SMALL_ONLY_OBSERVED),
        "small-only-observed") == 0);
    assert(strcmp(db_traffic_regime_state_name(
        DB_TRAFFIC_REGIME_OUT_ABOVE_1024_OBSERVED),
        "out-above-1024-observed") == 0);
    assert(strcmp(db_traffic_regime_state_name(
        DB_TRAFFIC_REGIME_OUT_65536_OBSERVED),
        "out-65536-observed") == 0);
    assert(strcmp(db_traffic_regime_state_name(DB_TRAFFIC_REGIME_FAILED),
        "failed") == 0);
    assert(strcmp(db_traffic_regime_state_name((DBTrafficRegimeState)99),
        "invalid-state") == 0);
    assert(strcmp(db_traffic_regime_result_name(DB_TRAFFIC_REGIME_OK),
        "ok") == 0);
    assert(strcmp(db_traffic_regime_result_name(
        DB_TRAFFIC_REGIME_INVALID_METADATA), "invalid-metadata") == 0);
    assert(strcmp(db_traffic_regime_result_name(
        DB_TRAFFIC_REGIME_BOUNDS_EXCEEDED), "bounds-exceeded") == 0);
    assert(strcmp(db_traffic_regime_result_name(DB_TRAFFIC_REGIME_CORRUPTED),
        "corrupted") == 0);
    assert(strcmp(db_traffic_regime_result_name(
        DB_TRAFFIC_REGIME_ALREADY_FINISHED), "already-finished") == 0);
    assert(strcmp(db_traffic_regime_result_name(
        DB_TRAFFIC_REGIME_STICKY_FAILURE), "sticky-failure") == 0);
    assert(strcmp(db_traffic_regime_result_name((DBTrafficRegimeResult)99),
        "invalid-result") == 0);
    return 0;
}
