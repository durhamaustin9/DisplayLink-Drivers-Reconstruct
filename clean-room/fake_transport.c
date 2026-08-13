#include "fake_transport.h"

#include <string.h>

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
    if (length > DB_FAKE_MAX_PACKET_SIZE) {
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
    }
    queue_clear(&fake->host_to_dock);
    queue_clear(&fake->dock_to_host);
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
        .connected = 1
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
    if (fake == NULL) {
        return;
    }
    fake->connected = 0;
    fake_close(fake);
}

void
db_fake_transport_reconnect(DBFakeTransport *fake)
{
    if (fake != NULL) {
        fake->connected = 1;
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
