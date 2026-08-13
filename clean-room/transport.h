#ifndef DOCKBRIDGE_TRANSPORT_H
#define DOCKBRIDGE_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    DB_TRANSPORT_KIND_FAKE = 1
} DBTransportKind;

typedef enum {
    DB_TRANSPORT_OK = 0,
    DB_TRANSPORT_INVALID_ARGUMENT,
    DB_TRANSPORT_CLOSED,
    DB_TRANSPORT_DISCONNECTED,
    DB_TRANSPORT_WRONG_ENDPOINT,
    DB_TRANSPORT_PACKET_TOO_LARGE,
    DB_TRANSPORT_WOULD_BLOCK
} DBTransportResult;

typedef struct {
    DBTransportResult (*open)(void *context);
    void (*close)(void *context);
    DBTransportResult (*write)(void *context, uint8_t endpoint,
        const uint8_t *bytes, size_t length);
    DBTransportResult (*read)(void *context, uint8_t endpoint,
        uint8_t *bytes, size_t capacity, size_t *length);
} DBTransportOperations;

typedef struct {
    DBTransportKind kind;
    const DBTransportOperations *operations;
    void *context;
} DBTransport;

DBTransportResult db_transport_open(DBTransport *transport);
void db_transport_close(DBTransport *transport);
DBTransportResult db_transport_write(DBTransport *transport,
    uint8_t endpoint, const uint8_t *bytes, size_t length);
DBTransportResult db_transport_read(DBTransport *transport,
    uint8_t endpoint, uint8_t *bytes, size_t capacity, size_t *length);
const char *db_transport_result_name(DBTransportResult result);

#endif
