#ifndef DOCKBRIDGE_CLEAN_ROOM_PROBE_MODEL_H
#define DOCKBRIDGE_CLEAN_ROOM_PROBE_MODEL_H

#include <stdint.h>

enum {
    DB_PROBE_VENDOR_ID = 0x17e9,
    DB_PROBE_PRODUCT_ID = 0x4323
};

typedef struct {
    uint32_t number;
    uint32_t interface_class;
    uint32_t subclass;
    uint32_t protocol;
    uint32_t alternate_setting;
    uint32_t endpoint_count;
} DBProbeInterface;

int db_probe_matches_device(uint32_t vendor_id, uint32_t product_id);
const char *db_probe_interface_role(const DBProbeInterface *descriptor);

#endif
