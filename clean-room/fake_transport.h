#ifndef DOCKBRIDGE_FAKE_TRANSPORT_H
#define DOCKBRIDGE_FAKE_TRANSPORT_H

#include "transport.h"

#include <stddef.h>
#include <stdint.h>

enum {
    DB_FAKE_ENDPOINT_OUT = 0x02,
    DB_FAKE_ENDPOINT_IN = 0x84,
    DB_FAKE_MAX_CHUNK_SIZE = 1024,
    DB_FAKE_QUEUE_CAPACITY = 8
};

typedef struct {
    size_t length;
    uint8_t bytes[DB_FAKE_MAX_CHUNK_SIZE];
} DBFakePacket;

typedef struct {
    DBFakePacket packets[DB_FAKE_QUEUE_CAPACITY];
    size_t head;
    size_t count;
} DBFakeQueue;

typedef struct {
    int connected;
    int open;
    uint64_t lifecycle_epoch;
    DBFakeQueue host_to_dock;
    DBFakeQueue dock_to_host;
    size_t open_count;
    size_t close_count;
    size_t write_attempt_count;
    size_t write_success_count;
    size_t read_attempt_count;
    size_t read_success_count;
} DBFakeTransport;

void db_fake_transport_initialize(DBFakeTransport *fake, DBTransport *transport);
void db_fake_transport_disconnect(DBFakeTransport *fake);
void db_fake_transport_reconnect(DBFakeTransport *fake);
DBTransportResult db_fake_transport_inject_inbound(DBFakeTransport *fake,
    const uint8_t *bytes, size_t length);
DBTransportResult db_fake_transport_take_outbound(DBFakeTransport *fake,
    uint8_t *bytes, size_t capacity, size_t *length);

#endif
