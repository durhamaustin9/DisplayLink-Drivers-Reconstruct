CC := /usr/bin/xcrun clang
CFLAGS := -std=c11 -Wall -Wextra -Werror -I.
BUILD_DIR := build
PROBE := $(BUILD_DIR)/dock-probe
CHECKER := $(BUILD_DIR)/transcript-check
PROBE_TEST := $(BUILD_DIR)/probe-model-tests
OBSERVATION_TEST := $(BUILD_DIR)/observation-model-tests

.PHONY: all probe checker test clean

all: probe checker

probe: $(PROBE)

$(BUILD_DIR):
	mkdir -p "$@"

$(PROBE): dock_probe.c probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) dock_probe.c probe_model.c -framework IOKit -framework CoreFoundation -o "$@"

checker: $(CHECKER)

$(CHECKER): transcript_check.c observation_model.c observation_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) transcript_check.c observation_model.c probe_model.c -o "$@"

$(PROBE_TEST): tests/probe_model_tests.c probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/probe_model_tests.c probe_model.c -o "$@"

$(OBSERVATION_TEST): tests/observation_model_tests.c observation_model.c observation_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/observation_model_tests.c observation_model.c probe_model.c -o "$@"

test: $(PROBE_TEST) $(OBSERVATION_TEST)
	"$(PROBE_TEST)"
	"$(OBSERVATION_TEST)"

clean:
	rm -rf "$(BUILD_DIR)"
