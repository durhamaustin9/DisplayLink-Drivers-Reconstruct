#include "fake_transport.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int
main(void)
{
    DBFakeTransport fake = {0};
    DBTransport transport = {0};
    uint8_t source[DB_FAKE_MAX_CHUNK_SIZE + 1U] = {0};
    uint8_t destination[DB_FAKE_MAX_CHUNK_SIZE] = {0};
    size_t length = 0;

    db_fake_transport_initialize(&fake, &transport);
    assert(!db_transport_is_open(&transport));
    assert(db_transport_lifecycle_epoch(&transport) != 0U);
    uint64_t initialized_epoch = db_transport_lifecycle_epoch(&transport);
    assert(transport.kind == DB_TRANSPORT_KIND_FAKE);
    assert(fake.connected);
    assert(!fake.open);

    source[0] = 0xa5;
    assert(db_transport_write(&transport, DB_FAKE_ENDPOINT_OUT,
        source, 1) == DB_TRANSPORT_CLOSED);
    assert(fake.write_attempt_count == 1);
    assert(db_transport_open(&transport) == DB_TRANSPORT_OK);
    assert(db_transport_is_open(&transport));
    uint64_t first_open_epoch = db_transport_lifecycle_epoch(&transport);
    assert(first_open_epoch != 0U);
    assert(first_open_epoch != initialized_epoch);
    assert(db_transport_open(&transport) == DB_TRANSPORT_OK);
    assert(db_transport_lifecycle_epoch(&transport) == first_open_epoch);
    assert(fake.open_count == 1);

    assert(db_transport_write(&transport, DB_FAKE_ENDPOINT_IN,
        source, 1) == DB_TRANSPORT_WRONG_ENDPOINT);
    assert(db_transport_write(&transport, DB_FAKE_ENDPOINT_OUT,
        source, sizeof(source)) == DB_TRANSPORT_PACKET_TOO_LARGE);

    for (size_t index = 0; index < DB_FAKE_QUEUE_CAPACITY; ++index) {
        source[0] = (uint8_t)index;
        assert(db_transport_write(&transport, DB_FAKE_ENDPOINT_OUT,
            source, 1) == DB_TRANSPORT_OK);
    }
    assert(db_transport_write(&transport, DB_FAKE_ENDPOINT_OUT,
        source, 1) == DB_TRANSPORT_WOULD_BLOCK);
    for (size_t index = 0; index < DB_FAKE_QUEUE_CAPACITY; ++index) {
        assert(db_fake_transport_take_outbound(&fake, destination,
            sizeof(destination), &length) == DB_TRANSPORT_OK);
        assert(length == 1);
        assert(destination[0] == (uint8_t)index);
    }
    assert(db_fake_transport_take_outbound(&fake, destination,
        sizeof(destination), &length) == DB_TRANSPORT_WOULD_BLOCK);
    assert(length == 0);

    source[0] = 0x42;
    source[1] = 0x24;
    assert(db_fake_transport_inject_inbound(&fake,
        source, 2) == DB_TRANSPORT_OK);
    assert(db_transport_read(&transport, DB_FAKE_ENDPOINT_OUT,
        destination, sizeof(destination), &length) ==
        DB_TRANSPORT_WRONG_ENDPOINT);
    assert(db_transport_read(&transport, DB_FAKE_ENDPOINT_IN,
        destination, 1, &length) == DB_TRANSPORT_PACKET_TOO_LARGE);
    assert(length == 2);
    assert(db_transport_read(&transport, DB_FAKE_ENDPOINT_IN,
        destination, sizeof(destination), &length) == DB_TRANSPORT_OK);
    assert(length == 2);
    assert(memcmp(source, destination, length) == 0);

    db_fake_transport_disconnect(&fake);
    uint64_t disconnected_epoch = db_transport_lifecycle_epoch(&transport);
    assert(disconnected_epoch != 0U);
    assert(disconnected_epoch != first_open_epoch);
    assert(!db_transport_is_open(&transport));
    assert(!fake.connected);
    assert(!fake.open);
    assert(db_transport_read(&transport, DB_FAKE_ENDPOINT_IN,
        destination, sizeof(destination), &length) ==
        DB_TRANSPORT_DISCONNECTED);
    assert(db_transport_open(&transport) == DB_TRANSPORT_DISCONNECTED);
    assert(db_transport_lifecycle_epoch(&transport) == disconnected_epoch);
    db_fake_transport_reconnect(&fake);
    uint64_t reconnected_epoch = db_transport_lifecycle_epoch(&transport);
    assert(reconnected_epoch != 0U);
    assert(reconnected_epoch != disconnected_epoch);
    assert(db_transport_open(&transport) == DB_TRANSPORT_OK);
    uint64_t second_open_epoch = db_transport_lifecycle_epoch(&transport);
    assert(second_open_epoch != 0U);
    assert(second_open_epoch != reconnected_epoch);
    assert(db_transport_is_open(&transport));
    db_transport_close(&transport);
    uint64_t closed_epoch = db_transport_lifecycle_epoch(&transport);
    assert(closed_epoch != 0U);
    assert(closed_epoch != second_open_epoch);
    assert(!db_transport_is_open(&transport));
    db_transport_close(&transport);
    assert(db_transport_lifecycle_epoch(&transport) == closed_epoch);
    assert(fake.close_count == 2);

    db_fake_transport_initialize(&fake, &transport);
    assert(db_transport_lifecycle_epoch(&transport) != 0U);
    assert(db_transport_lifecycle_epoch(&transport) != closed_epoch);

    assert(db_transport_open(NULL) == DB_TRANSPORT_INVALID_ARGUMENT);
    assert(!db_transport_is_open(NULL));
    assert(db_transport_lifecycle_epoch(NULL) == 0U);
    assert(db_transport_write(&transport, DB_FAKE_ENDPOINT_OUT,
        NULL, 1) == DB_TRANSPORT_INVALID_ARGUMENT);
    assert(db_transport_read(&transport, DB_FAKE_ENDPOINT_IN,
        NULL, 1, &length) == DB_TRANSPORT_INVALID_ARGUMENT);
    assert(strcmp(db_transport_result_name(DB_TRANSPORT_WOULD_BLOCK),
        "would-block") == 0);
    assert(strcmp(db_transport_result_name((DBTransportResult)99),
        "invalid-result") == 0);
    return 0;
}
