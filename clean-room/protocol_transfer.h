#ifndef DOCKBRIDGE_PROTOCOL_TRANSFER_H
#define DOCKBRIDGE_PROTOCOL_TRANSFER_H

#include <stddef.h>
#include <stdint.h>

enum {
    DB_PROTOCOL_IN_PREFIX_SIZE = 4,
    DB_PROTOCOL_MAX_TRANSFER_LENGTH = 65536
};

/*
 * This type deliberately contains transfer-envelope metadata only. It cannot
 * retain, expose, or replay a captured transfer body.
 */
typedef enum {
    DB_PROTOCOL_DIRECTION_INVALID = 0,
    DB_PROTOCOL_DIRECTION_OUT,
    DB_PROTOCOL_DIRECTION_IN
} DBProtocolDirection;

/* The qualified clean-room observations currently cover bulk transfers only. */
typedef enum {
    DB_PROTOCOL_TRANSFER_KIND_INVALID = 0,
    DB_PROTOCOL_TRANSFER_KIND_BULK
} DBProtocolTransferKind;

typedef struct {
    DBProtocolDirection direction;
    uint8_t endpoint;
    DBProtocolTransferKind kind;
    uint8_t succeeded;
    size_t length;
    uint8_t in_prefix[DB_PROTOCOL_IN_PREFIX_SIZE];
} DBProtocolTransferMetadata;

/*
 * Validates the bounded, direction-sensitive envelope. For an OUT transfer,
 * in_prefix must be all zero. For an IN transfer, the first two prefix bytes
 * must be zero and the final two must encode length - 4 as little-endian
 * unsigned metadata. No payload pointer is accepted by this API.
 */
int db_protocol_transfer_metadata_is_valid(
    const DBProtocolTransferMetadata *transfer);

#endif
