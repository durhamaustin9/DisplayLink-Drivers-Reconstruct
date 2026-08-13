#ifndef DISPLAYLINK_PATH_POLICY_H
#define DISPLAYLINK_PATH_POLICY_H

#include <stdbool.h>

bool dl_path_is_exact_engine_executable(const char *engine_root, const char *candidate);
bool dl_path_is_known_displaylink_executable(const char *candidate);
bool dl_name_is_known_displaylink_executable(const char *candidate);

#endif
