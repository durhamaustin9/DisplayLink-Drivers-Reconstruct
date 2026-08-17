#include "exchange_parser.h"
#include "fake_transport.h"
#include "transport.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    DBExchangeDirection direction;
    size_t length;
} LabFrame;

static const LabFrame synthetic_phase_a[DB_EXCHANGE_PHASE_A_FRAME_COUNT] = {
    {DB_EXCHANGE_DIRECTION_OUT, 16},
    {DB_EXCHANGE_DIRECTION_OUT, 32},
    {DB_EXCHANGE_DIRECTION_OUT, 80},
    {DB_EXCHANGE_DIRECTION_IN, 39},
    {DB_EXCHANGE_DIRECTION_OUT, 48},
    {DB_EXCHANGE_DIRECTION_IN, 38},
    {DB_EXCHANGE_DIRECTION_OUT, 64},
    {DB_EXCHANGE_DIRECTION_IN, 38},
    {DB_EXCHANGE_DIRECTION_OUT, 64},
    {DB_EXCHANGE_DIRECTION_IN, 38},
    {DB_EXCHANGE_DIRECTION_IN, 549},
    {DB_EXCHANGE_DIRECTION_IN, 31},
    {DB_EXCHANGE_DIRECTION_OUT, 176},
    {DB_EXCHANGE_DIRECTION_IN, 38},
    {DB_EXCHANGE_DIRECTION_IN, 34}
};

_Static_assert(DB_FAKE_ENDPOINT_OUT == DB_EXCHANGE_ENDPOINT_OUT,
    "fake and parser OUT endpoints must agree");
_Static_assert(DB_FAKE_ENDPOINT_IN == DB_EXCHANGE_ENDPOINT_IN,
    "fake and parser IN endpoints must agree");

static void
make_synthetic_frame(uint8_t *bytes, const LabFrame *frame)
{
    memset(bytes, 0xa5, frame->length);
    if (frame->direction == DB_EXCHANGE_DIRECTION_IN) {
        size_t body_length = frame->length - DB_EXCHANGE_IN_HEADER_SIZE;
        bytes[0] = 0;
        bytes[1] = 0;
        bytes[2] = (uint8_t)(body_length & 0xffU);
        bytes[3] = (uint8_t)((body_length >> 8U) & 0xffU);
    }
}

int
main(void)
{
    DBFakeTransport fake = {0};
    DBTransport transport = {0};
    DBExchangeParser parser = {0};
    uint8_t source[DB_EXCHANGE_MAX_FRAME_SIZE] = {0};
    uint8_t received[DB_EXCHANGE_MAX_FRAME_SIZE] = {0};
    size_t received_length = 0;
    size_t trial_variable_windows = 0;
    size_t trial_variable_bytes = 0;

    db_fake_transport_initialize(&fake, &transport);
    db_exchange_parser_initialize(&parser);
    puts("DockBridge first-burst parser lab");
    puts("Synthetic bytes and in-memory transport only; no device access.");
    if (db_transport_open(&transport) != DB_TRANSPORT_OK) {
        fputs("exchange-lab: fake transport did not open\n", stderr);
        return 1;
    }

    for (size_t index = 0; index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++index) {
        const LabFrame *frame = &synthetic_phase_a[index];
        make_synthetic_frame(source, frame);
        DBTransportResult transport_result;
        if (frame->direction == DB_EXCHANGE_DIRECTION_OUT) {
            transport_result = db_transport_write(&transport,
                DB_FAKE_ENDPOINT_OUT, source, frame->length);
            if (transport_result == DB_TRANSPORT_OK) {
                transport_result = db_fake_transport_take_outbound(&fake,
                    received, sizeof(received), &received_length);
            }
        } else {
            transport_result = db_fake_transport_inject_inbound(&fake,
                source, frame->length);
            if (transport_result == DB_TRANSPORT_OK) {
                transport_result = db_transport_read(&transport,
                    DB_FAKE_ENDPOINT_IN, received, sizeof(received),
                    &received_length);
            }
        }
        if (transport_result != DB_TRANSPORT_OK ||
            received_length != frame->length ||
            db_exchange_parser_accept(&parser, frame->direction,
                frame->direction == DB_EXCHANGE_DIRECTION_OUT ?
                    DB_EXCHANGE_ENDPOINT_OUT : DB_EXCHANGE_ENDPOINT_IN,
                received, received_length) != DB_EXCHANGE_OK) {
            fputs("exchange-lab: synthetic exchange failed\n", stderr);
            db_transport_close(&transport);
            return 1;
        }
    }

    db_transport_close(&transport);
    for (size_t index = 0; index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++index) {
        DBExchangeTransferStructure structure = {0};
        if (db_exchange_phase_a_structure(index, &structure) !=
            DB_EXCHANGE_OK) {
            fputs("exchange-lab: structure metadata failed\n", stderr);
            return 1;
        }
        if (structure.trial_variable_length > 0) {
            ++trial_variable_windows;
            trial_variable_bytes += structure.trial_variable_length;
        }
    }
    printf("frames=%zu writes=%zu reads=%zu parser-state=%s\n",
        parser.next_frame_index, fake.write_success_count,
        fake.read_success_count, db_exchange_state_name(parser.state));
    printf("trial-variable-windows=%zu trial-variable-bytes=%zu\n",
        trial_variable_windows, trial_variable_bytes);
    if (parser.state != DB_EXCHANGE_COMPLETE ||
        fake.write_success_count != 7 || fake.read_success_count != 8 ||
        trial_variable_windows != 3 || trial_variable_bytes != 144) {
        fputs("exchange-lab: completion invariant failed\n", stderr);
        return 1;
    }
    puts("PASS: observed burst parsed with synthetic transfers; hardware remains disabled.");
    return 0;
}
