CC := /usr/bin/xcrun clang
CFLAGS := -std=c11 -Wall -Wextra -Werror -I.
BUILD_DIR := build
PROBE := $(BUILD_DIR)/dock-probe
TEST := $(BUILD_DIR)/probe-model-tests

.PHONY: all probe test clean

all: probe

probe: $(PROBE)

$(BUILD_DIR):
	mkdir -p "$@"

$(PROBE): dock_probe.c probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) dock_probe.c probe_model.c -framework IOKit -framework CoreFoundation -o "$@"

$(TEST): tests/probe_model_tests.c probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/probe_model_tests.c probe_model.c -o "$@"

test: $(TEST)
	"$(TEST)"

clean:
	rm -rf "$(BUILD_DIR)"
