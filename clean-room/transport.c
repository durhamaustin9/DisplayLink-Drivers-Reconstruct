#include "transport.h"

DBTransportResult
db_transport_open(DBTransport *transport)
{
    if (transport == NULL || transport->operations == NULL ||
        transport->operations->open == NULL || transport->context == NULL) {
        return DB_TRANSPORT_INVALID_ARGUMENT;
    }
    return transport->operations->open(transport->context);
}

void
db_transport_close(DBTransport *transport)
{
    if (transport != NULL && transport->operations != NULL &&
        transport->operations->close != NULL && transport->context != NULL) {
        transport->operations->close(transport->context);
    }
}

DBTransportResult
db_transport_write(DBTransport *transport, uint8_t endpoint,
    const uint8_t *bytes, size_t length)
{
    if (transport == NULL || transport->operations == NULL ||
        transport->operations->write == NULL || transport->context == NULL ||
        bytes == NULL || length == 0) {
        return DB_TRANSPORT_INVALID_ARGUMENT;
    }
    return transport->operations->write(
        transport->context, endpoint, bytes, length);
}

DBTransportResult
db_transport_read(DBTransport *transport, uint8_t endpoint,
    uint8_t *bytes, size_t capacity, size_t *length)
{
    if (transport == NULL || transport->operations == NULL ||
        transport->operations->read == NULL || transport->context == NULL ||
        bytes == NULL || capacity == 0 || length == NULL) {
        return DB_TRANSPORT_INVALID_ARGUMENT;
    }
    return transport->operations->read(
        transport->context, endpoint, bytes, capacity, length);
}

const char *
db_transport_result_name(DBTransportResult result)
{
    switch (result) {
    case DB_TRANSPORT_OK:
        return "ok";
    case DB_TRANSPORT_INVALID_ARGUMENT:
        return "invalid-argument";
    case DB_TRANSPORT_CLOSED:
        return "closed";
    case DB_TRANSPORT_DISCONNECTED:
        return "disconnected";
    case DB_TRANSPORT_WRONG_ENDPOINT:
        return "wrong-endpoint";
    case DB_TRANSPORT_PACKET_TOO_LARGE:
        return "packet-too-large";
    case DB_TRANSPORT_WOULD_BLOCK:
        return "would-block";
    }
    return "invalid-result";
}
