#include "fake_transport.h"

#include <stdatomic.h>
#include <string.h>

static _Atomic uint64_t next_lifecycle_epoch = UINT64_C(1);

static uint64_t
take_lifecycle_epoch(void)
{
    uint64_t current = atomic_load_explicit(&next_lifecycle_epoch,
        memory_order_relaxed);
    for (;;) {
        if (current == UINT64_MAX) {
            return 0U;
        }
        uint64_t next = current + UINT64_C(1);
        if (atomic_compare_exchange_weak_explicit(&next_lifecycle_epoch,
                &current, next, memory_order_relaxed,
                memory_order_relaxed)) {
            return current;
        }
    }
}

static void
advance_lifecycle_epoch(DBFakeTransport *fake)
{
    fake->lifecycle_epoch = take_lifecycle_epoch();
}

static void
queue_clear(DBFakeQueue *queue)
{
    *queue = (DBFakeQueue) {0};
}

static DBTransportResult
queue_push(DBFakeQueue *queue, const uint8_t *bytes, size_t length)
{
    if (queue == NULL || bytes == NULL || length == 0) {
        return DB_TRANSPORT_INVALID_ARGUMENT;
    }
    if (length > DB_FAKE_MAX_CHUNK_SIZE) {
        return DB_TRANSPORT_PACKET_TOO_LARGE;
    }
    if (queue->count >= DB_FAKE_QUEUE_CAPACITY) {
        return DB_TRANSPORT_WOULD_BLOCK;
    }

    size_t index = (queue->head + queue->count) % DB_FAKE_QUEUE_CAPACITY;
    queue->packets[index].length = length;
    memcpy(queue->packets[index].bytes, bytes, length);
    ++queue->count;
    return DB_TRANSPORT_OK;
}

static DBTransportResult
queue_pop(DBFakeQueue *queue, uint8_t *bytes, size_t capacity, size_t *length)
{
    if (queue == NULL || bytes == NULL || capacity == 0 || length == NULL) {
        return DB_TRANSPORT_INVALID_ARGUMENT;
    }
    if (queue->count == 0) {
        *length = 0;
        return DB_TRANSPORT_WOULD_BLOCK;
    }

    DBFakePacket *packet = &queue->packets[queue->head];
    if (capacity < packet->length) {
        *length = packet->length;
        return DB_TRANSPORT_PACKET_TOO_LARGE;
    }
    memcpy(bytes, packet->bytes, packet->length);
    *length = packet->length;
    packet->length = 0;
    queue->head = (queue->head + 1U) % DB_FAKE_QUEUE_CAPACITY;
    --queue->count;
    return DB_TRANSPORT_OK;
}

static DBTransportResult
fake_open(void *context)
{
    DBFakeTransport *fake = context;
    if (!fake->connected) {
        return DB_TRANSPORT_DISCONNECTED;
    }
    if (!fake->open) {
        fake->open = 1;
        ++fake->open_count;
        advance_lifecycle_epoch(fake);
    }
    return DB_TRANSPORT_OK;
}

static void
fake_close(void *context)
{
    DBFakeTransport *fake = context;
    if (fake->open) {
        fake->open = 0;
        ++fake->close_count;
        advance_lifecycle_epoch(fake);
    }
    queue_clear(&fake->host_to_dock);
    queue_clear(&fake->dock_to_host);
}

static int
fake_is_open(const void *context)
{
    const DBFakeTransport *fake = context;
    return fake != NULL && fake->connected && fake->open;
}

static uint64_t
fake_lifecycle_epoch(const void *context)
{
    const DBFakeTransport *fake = context;
    return fake == NULL ? 0U : fake->lifecycle_epoch;
}

static DBTransportResult
fake_write(void *context, uint8_t endpoint, const uint8_t *bytes, size_t length)
{
    DBFakeTransport *fake = context;
    ++fake->write_attempt_count;
    if (!fake->connected) {
        return DB_TRANSPORT_DISCONNECTED;
    }
    if (!fake->open) {
        return DB_TRANSPORT_CLOSED;
    }
    if (endpoint != DB_FAKE_ENDPOINT_OUT) {
        return DB_TRANSPORT_WRONG_ENDPOINT;
    }
    DBTransportResult result = queue_push(&fake->host_to_dock, bytes, length);
    if (result == DB_TRANSPORT_OK) {
        ++fake->write_success_count;
    }
    return result;
}

static DBTransportResult
fake_read(void *context, uint8_t endpoint, uint8_t *bytes,
    size_t capacity, size_t *length)
{
    DBFakeTransport *fake = context;
    ++fake->read_attempt_count;
    if (!fake->connected) {
        return DB_TRANSPORT_DISCONNECTED;
    }
    if (!fake->open) {
        return DB_TRANSPORT_CLOSED;
    }
    if (endpoint != DB_FAKE_ENDPOINT_IN) {
        return DB_TRANSPORT_WRONG_ENDPOINT;
    }
    DBTransportResult result = queue_pop(
        &fake->dock_to_host, bytes, capacity, length);
    if (result == DB_TRANSPORT_OK) {
        ++fake->read_success_count;
    }
    return result;
}

static const DBTransportOperations fake_operations = {
    .open = fake_open,
    .close = fake_close,
    .is_open = fake_is_open,
    .lifecycle_epoch = fake_lifecycle_epoch,
    .write = fake_write,
    .read = fake_read
};

void
db_fake_transport_initialize(DBFakeTransport *fake, DBTransport *transport)
{
    if (fake == NULL || transport == NULL) {
        return;
    }
    *fake = (DBFakeTransport) {
        .connected = 1,
        .lifecycle_epoch = take_lifecycle_epoch()
    };
    *transport = (DBTransport) {
        .kind = DB_TRANSPORT_KIND_FAKE,
        .operations = &fake_operations,
        .context = fake
    };
}

void
db_fake_transport_disconnect(DBFakeTransport *fake)
{
    if (fake == NULL || !fake->connected) {
        return;
    }
    fake->connected = 0;
    if (fake->open) {
        fake->open = 0;
        ++fake->close_count;
    }
    queue_clear(&fake->host_to_dock);
    queue_clear(&fake->dock_to_host);
    advance_lifecycle_epoch(fake);
}

void
db_fake_transport_reconnect(DBFakeTransport *fake)
{
    if (fake != NULL && !fake->connected) {
        fake->connected = 1;
        advance_lifecycle_epoch(fake);
    }
}

DBTransportResult
db_fake_transport_inject_inbound(DBFakeTransport *fake,
    const uint8_t *bytes, size_t length)
{
    if (fake == NULL || !fake->connected) {
        return fake == NULL ? DB_TRANSPORT_INVALID_ARGUMENT :
            DB_TRANSPORT_DISCONNECTED;
    }
    return queue_push(&fake->dock_to_host, bytes, length);
}

DBTransportResult
db_fake_transport_take_outbound(DBFakeTransport *fake,
    uint8_t *bytes, size_t capacity, size_t *length)
{
    if (fake == NULL || !fake->connected) {
        return fake == NULL ? DB_TRANSPORT_INVALID_ARGUMENT :
            DB_TRANSPORT_DISCONNECTED;
    }
    return queue_pop(&fake->host_to_dock, bytes, capacity, length);
}
