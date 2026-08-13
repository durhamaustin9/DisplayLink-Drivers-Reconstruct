#include "probe_model.h"

#include <assert.h>
#include <string.h>

int
main(void)
{
    DBProbeInterface display = {0, 0xff, 0, 3, 0, 2};
    DBProbeInterface auxiliary = {1, 0xfe, 1, 1, 0, 0};
    DBProbeInterface audio = {2, 1, 1, 0, 0, 1};
    DBProbeInterface network = {5, 2, 13, 0, 0, 1};
    DBProbeInterface unknown = {9, 0xff, 42, 42, 0, 4};

    assert(db_probe_matches_device(0x17e9, 0x4323));
    assert(!db_probe_matches_device(0x17e9, 0x6006));
    assert(!db_probe_matches_device(0x1234, 0x4323));
    assert(strstr(db_probe_interface_role(&display), "display transport") != 0);
    assert(strstr(db_probe_interface_role(&auxiliary), "auxiliary") != 0);
    assert(strstr(db_probe_interface_role(&audio), "audio") != 0);
    assert(strstr(db_probe_interface_role(&network), "networking") != 0);
    assert(strstr(db_probe_interface_role(&unknown), "unclassified") != 0);
    assert(strcmp(db_probe_interface_role(0), "invalid") == 0);
    return 0;
}
