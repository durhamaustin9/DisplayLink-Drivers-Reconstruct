#ifndef DISPLAYLINK_CLEAN_ROOM_PROBE_MODEL_H
#define DISPLAYLINK_CLEAN_ROOM_PROBE_MODEL_H

#include <stdint.h>

enum {
    DL_PROBE_VENDOR_ID = 0x17e9,
    DL_PROBE_PRODUCT_ID = 0x4323
};

typedef struct {
    uint32_t number;
    uint32_t interface_class;
    uint32_t subclass;
    uint32_t protocol;
    uint32_t alternate_setting;
    uint32_t endpoint_count;
} DLProbeInterface;

int dl_probe_matches_device(uint32_t vendor_id, uint32_t product_id);
const char *dl_probe_interface_role(const DLProbeInterface *descriptor);

#endif
