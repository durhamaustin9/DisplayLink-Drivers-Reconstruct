#include "path_policy.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static const char allowed_relative_path[] = "/Contents/MacOS/DockBridgeEngine";
static const char displaylink_component_token[] = "displaylink";

static char dl_ascii_lowercase(char character)
{
    if (character >= 'A' && character <= 'Z') {
        return (char)(character + ('a' - 'A'));
    }
    return character;
}

static bool dl_component_contains_displaylink(const char *start, const char *end)
{
    size_t token_length = sizeof(displaylink_component_token) - 1U;
    if (start == NULL || end == NULL || end < start ||
        (size_t)(end - start) < token_length) {
        return false;
    }

    for (const char *position = start;
         (size_t)(end - position) >= token_length; ++position) {
        size_t index = 0;
        while (index < token_length &&
            dl_ascii_lowercase(position[index]) == displaylink_component_token[index]) {
            ++index;
        }
        if (index == token_length) {
            return true;
        }
    }
    return false;
}

static bool dl_parent_path_has_displaylink_component(
    const char *candidate, const char *basename_separator)
{
    const char *component = candidate + 1;
    while (component < basename_separator) {
        const char *separator = memchr(component, '/',
            (size_t)(basename_separator - component));
        const char *component_end = separator == NULL ? basename_separator : separator;
        if (dl_component_contains_displaylink(component, component_end)) {
            return true;
        }
        if (separator == NULL) {
            break;
        }
        component = separator + 1;
    }
    return false;
}

bool dl_path_is_exact_engine_executable(const char *engine_root, const char *candidate)
{
    if (engine_root == NULL || candidate == NULL || engine_root[0] != '/' || candidate[0] != '/') {
        return false;
    }

    size_t root_length = strlen(engine_root);
    while (root_length > 1 && engine_root[root_length - 1] == '/') {
        --root_length;
    }
    if (root_length > (size_t)INT_MAX) {
        return false;
    }

    char expected[PATH_MAX];
    int written = snprintf(expected, sizeof(expected), "%.*s%s",
        (int)root_length, engine_root, allowed_relative_path);
    return written > 0 && (size_t)written < sizeof(expected) &&
        strcmp(expected, candidate) == 0;
}

bool dl_path_is_known_displaylink_executable(const char *candidate)
{
    if (candidate == NULL || candidate[0] != '/') {
        return false;
    }

    const char *basename_separator = strrchr(candidate, '/');
    if (basename_separator == NULL || basename_separator[1] == '\0') {
        return false;
    }
    const char *basename = basename_separator + 1;

    if (strcmp(basename, "DockBridgeEngine") == 0 ||
        strcmp(basename, "DisplayLinkUserAgent") == 0 ||
        strcmp(basename, "DisplayLinkXpcService") == 0 ||
        strcmp(basename, "DisplayLinkLoginHelper") == 0) {
        return true;
    }
    return strcmp(basename, "CrashRestartHelper") == 0 &&
        dl_parent_path_has_displaylink_component(candidate, basename_separator);
}

bool dl_name_is_known_displaylink_executable(const char *candidate)
{
    if (candidate == NULL || candidate[0] == '\0') {
        return false;
    }

    /*
     * proc_bsdinfo may truncate a command name. DisplayLink's executable names
     * all retain this distinctive prefix when truncated. CrashRestartHelper is
     * intentionally excluded here because that generic name is also used by
     * unrelated products; it is recognized only when its resolved path has a
     * DisplayLink parent component.
     */
    return strcmp(candidate, "DockBridgeEngine") == 0 ||
        strncmp(candidate, "DisplayLink", sizeof("DisplayLink") - 1U) == 0;
}
