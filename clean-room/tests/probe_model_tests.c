#include "probe_model.h"

#include <assert.h>
#include <string.h>

int
main(void)
{
    DLProbeInterface display = {0, 0xff, 0, 3, 0, 2};
    DLProbeInterface auxiliary = {1, 0xfe, 1, 1, 0, 0};
    DLProbeInterface audio = {2, 1, 1, 0, 0, 1};
    DLProbeInterface network = {5, 2, 13, 0, 0, 1};
    DLProbeInterface unknown = {9, 0xff, 42, 42, 0, 4};

    assert(dl_probe_matches_device(0x17e9, 0x4323));
    assert(!dl_probe_matches_device(0x17e9, 0x6006));
    assert(!dl_probe_matches_device(0x1234, 0x4323));
    assert(strstr(dl_probe_interface_role(&display), "display transport") != 0);
    assert(strstr(dl_probe_interface_role(&auxiliary), "auxiliary") != 0);
    assert(strstr(dl_probe_interface_role(&audio), "audio") != 0);
    assert(strstr(dl_probe_interface_role(&network), "networking") != 0);
    assert(strstr(dl_probe_interface_role(&unknown), "unclassified") != 0);
    assert(strcmp(dl_probe_interface_role(0), "invalid") == 0);
    return 0;
}
