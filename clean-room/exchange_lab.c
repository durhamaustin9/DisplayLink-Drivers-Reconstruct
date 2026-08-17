#include "exchange_parser.h"
#include "fake_transport.h"
#include "transport.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    DBExchangeDirection direction;
    size_t length;
} LabFrame;

typedef struct {
    DBExchangeOrderVariant order_variant;
    size_t frame_count;
    size_t write_count;
    size_t read_count;
} LabRun;

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

static int
run_observed_order(DBExchangeOrderVariant requested_variant, LabRun *run)
{
    DBFakeTransport fake = {0};
    DBTransport transport = {0};
    DBExchangeParser parser = {0};
    uint8_t source[DB_EXCHANGE_MAX_FRAME_SIZE] = {0};
    uint8_t received[DB_EXCHANGE_MAX_FRAME_SIZE] = {0};
    size_t received_length = 0;

    if (run == NULL ||
        (requested_variant != DB_EXCHANGE_ORDER_CANONICAL &&
            requested_variant != DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED)) {
        return 0;
    }

    db_fake_transport_initialize(&fake, &transport);
    db_exchange_parser_initialize(&parser);
    if (db_transport_open(&transport) != DB_TRANSPORT_OK) {
        return 0;
    }

    for (size_t observed_index = 0;
         observed_index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++observed_index) {
        size_t role_index = observed_index;
        if (requested_variant == DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED) {
            if (observed_index == DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX) {
                role_index = DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX;
            } else if (observed_index ==
                DB_EXCHANGE_SWAP_SECOND_ROLE_INDEX) {
                role_index = DB_EXCHANGE_SWAP_FIRST_ROLE_INDEX;
            }
        }
        const LabFrame *frame = &synthetic_phase_a[role_index];
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
            return 0;
        }
    }

    db_transport_close(&transport);
    if (parser.state != DB_EXCHANGE_COMPLETE ||
        parser.next_frame_index != DB_EXCHANGE_PHASE_A_FRAME_COUNT ||
        parser.order_variant != requested_variant ||
        parser.swapped_pair_pending != 0 ||
        fake.write_success_count != 7 || fake.read_success_count != 8) {
        return 0;
    }
    *run = (LabRun) {
        .order_variant = parser.order_variant,
        .frame_count = parser.next_frame_index,
        .write_count = fake.write_success_count,
        .read_count = fake.read_success_count
    };
    return 1;
}

int
main(void)
{
    size_t observed_variable_ranges = 0;
    size_t observed_variable_bytes = 0;
    LabRun canonical = {0};
    LabRun swapped = {0};

    puts("DockBridge first-burst parser lab");
    puts("Synthetic bytes and in-memory transport only; no device access.");
    if (!run_observed_order(DB_EXCHANGE_ORDER_CANONICAL, &canonical) ||
        !run_observed_order(DB_EXCHANGE_ORDER_ROLES_9_10_SWAPPED, &swapped)) {
        fputs("exchange-lab: observed-order replay failed\n", stderr);
        return 1;
    }

    for (size_t index = 0; index < DB_EXCHANGE_PHASE_A_FRAME_COUNT; ++index) {
        DBExchangeTransferStructure structure = {0};
        if (db_exchange_phase_a_structure(index, &structure) !=
            DB_EXCHANGE_OK) {
            fputs("exchange-lab: structure metadata failed\n", stderr);
            return 1;
        }
        if (structure.observed_variable_range_count >
            DB_EXCHANGE_MAX_VARIABLE_RANGES_PER_ROLE) {
            fputs("exchange-lab: variable-range count is invalid\n", stderr);
            return 1;
        }
        size_t previous_range_end = 0;
        for (size_t range_index = 0;
             range_index < structure.observed_variable_range_count;
             ++range_index) {
            const DBExchangeObservedVariableRange *range =
                &structure.observed_variable_ranges[range_index];
            if (range->length == 0 || range->offset > structure.length ||
                range->length > structure.length - range->offset ||
                (range_index > 0 && previous_range_end >= range->offset)) {
                fputs("exchange-lab: variable range is invalid\n", stderr);
                return 1;
            }
            previous_range_end = range->offset + range->length;
            ++observed_variable_ranges;
            observed_variable_bytes += range->length;
        }
        for (size_t range_index = structure.observed_variable_range_count;
             range_index < DB_EXCHANGE_MAX_VARIABLE_RANGES_PER_ROLE;
             ++range_index) {
            if (structure.observed_variable_ranges[range_index].offset != 0 ||
                structure.observed_variable_ranges[range_index].length != 0) {
                fputs("exchange-lab: unused variable range is nonzero\n",
                    stderr);
                return 1;
            }
        }
    }
    printf("order=%s frames=%zu writes=%zu reads=%zu\n",
        db_exchange_order_variant_name(canonical.order_variant),
        canonical.frame_count, canonical.write_count, canonical.read_count);
    printf("order=%s frames=%zu writes=%zu reads=%zu\n",
        db_exchange_order_variant_name(swapped.order_variant),
        swapped.frame_count, swapped.write_count, swapped.read_count);
    printf("observed-variable-ranges=%zu observed-variable-bytes=%zu\n",
        observed_variable_ranges, observed_variable_bytes);
    if (observed_variable_ranges != 6 || observed_variable_bytes != 147) {
        fputs("exchange-lab: completion invariant failed\n", stderr);
        return 1;
    }
    puts("PASS: both observed orders parsed with synthetic transfers; hardware remains disabled.");
    return 0;
}
