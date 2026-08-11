#include "../path_policy.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    const char *root = "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app";

    assert(dl_path_is_exact_engine_executable(root,
        "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app/Contents/MacOS/DisplayLinkUserAgent"));
    assert(dl_path_is_exact_engine_executable(
        "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app/",
        "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app/Contents/MacOS/DisplayLinkUserAgent"));

    assert(!dl_path_is_exact_engine_executable(root,
        "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app.evil/Contents/MacOS/DisplayLinkUserAgent"));
    assert(!dl_path_is_exact_engine_executable(root,
        "/Applications/DisplayLink Manager.app/Contents/MacOS/DisplayLinkUserAgent"));
    assert(!dl_path_is_exact_engine_executable(root,
        "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app/Contents/MacOS/DisplayLinkXpcService"));
    assert(!dl_path_is_exact_engine_executable(root,
        "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app/Contents/MacOS/CrashRestartHelper"));
    assert(!dl_path_is_exact_engine_executable(root,
        "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app/Contents/MacOS/Unrelated"));
    assert(!dl_path_is_exact_engine_executable("relative/path", "/relative/path/Contents/MacOS/DisplayLinkUserAgent"));
    assert(!dl_path_is_exact_engine_executable(NULL,
        "/opt/displaylink-test/Contained.app/Contents/Helpers/DisplayLink Core Engine.app/Contents/MacOS/DisplayLinkUserAgent"));
    assert(!dl_path_is_exact_engine_executable(root, NULL));

    assert(dl_path_is_known_displaylink_executable(
        "/Applications/DisplayLink Manager.app/Contents/MacOS/DisplayLinkUserAgent"));
    assert(dl_path_is_known_displaylink_executable(
        "/Applications/DisplayLink Manager.app/Contents/Helpers/DisplayLinkXpcService"));
    assert(dl_path_is_known_displaylink_executable(
        "/Applications/DisplayLink Manager.app/Contents/Helpers/CrashRestartHelper"));
    assert(dl_path_is_known_displaylink_executable(
        "/Applications/displaylink-local.app/Contents/Helpers/CrashRestartHelper"));
    assert(dl_path_is_known_displaylink_executable(
        "/Applications/DisplayLink Manager.app/Contents/Library/LoginItems/DisplayLinkLoginHelper"));
    assert(!dl_path_is_known_displaylink_executable(
        "/Applications/OtherVendor.app/Contents/Helpers/CrashRestartHelper"));
    assert(!dl_path_is_known_displaylink_executable(
        "/Applications/DisplayLink Manager.app/Contents/MacOS/DisplayLinkUserAgent.evil"));
    assert(!dl_path_is_known_displaylink_executable(
        "/Applications/DisplayLink Manager.app/Contents/MacOS/EvilDisplayLinkUserAgent"));
    assert(!dl_path_is_known_displaylink_executable(
        "/Applications/DisplayLinkUserAgent/Contents/MacOS/Unrelated"));
    assert(!dl_path_is_known_displaylink_executable(
        "relative/DisplayLinkUserAgent"));
    assert(!dl_path_is_known_displaylink_executable("/"));
    assert(!dl_path_is_known_displaylink_executable(NULL));
    return 0;
}
