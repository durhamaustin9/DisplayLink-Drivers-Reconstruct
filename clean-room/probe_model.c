#include "probe_model.h"

int
db_probe_matches_device(uint32_t vendor_id, uint32_t product_id)
{
    return vendor_id == DB_PROBE_VENDOR_ID && product_id == DB_PROBE_PRODUCT_ID;
}

const char *
db_probe_interface_role(const DBProbeInterface *descriptor)
{
    if (descriptor == 0) {
        return "invalid";
    }

    if (descriptor->number == 0 && descriptor->interface_class == 0xff &&
        descriptor->subclass == 0 && descriptor->protocol == 3) {
        return "candidate proprietary display transport (unopened)";
    }
    if (descriptor->number == 1 && descriptor->interface_class == 0xfe &&
        descriptor->subclass == 1 && descriptor->protocol == 1) {
        return "vendor auxiliary interface (unopened)";
    }
    if (descriptor->interface_class == 1) {
        return "standard USB audio; leave to macOS";
    }
    if (descriptor->interface_class == 2 || descriptor->interface_class == 10) {
        return "standard USB networking; leave to macOS";
    }
    return "unclassified; leave unopened";
}
