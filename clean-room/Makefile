CC := /usr/bin/xcrun clang
CFLAGS := -std=c11 -Wall -Wextra -Werror -I.
BUILD_DIR := build
PROBE := $(BUILD_DIR)/dock-probe
CHECKER := $(BUILD_DIR)/transcript-check
DESCRIPTOR_READER := $(BUILD_DIR)/standard-descriptor-reader
PROBE_TEST := $(BUILD_DIR)/probe-model-tests
OBSERVATION_TEST := $(BUILD_DIR)/observation-model-tests
DESCRIPTOR_TEST := $(BUILD_DIR)/descriptor-model-tests

.PHONY: all probe checker descriptors test clean

all: probe checker

probe: $(PROBE)

$(BUILD_DIR):
	mkdir -p "$@"

$(PROBE): dock_probe.c probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) dock_probe.c probe_model.c -framework IOKit -framework CoreFoundation -o "$@"

checker: $(CHECKER)

$(CHECKER): transcript_check.c observation_model.c observation_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) transcript_check.c observation_model.c probe_model.c -o "$@"

descriptors: $(DESCRIPTOR_READER)

$(DESCRIPTOR_READER): standard_descriptor_reader.m descriptor_model.c descriptor_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fobjc-arc standard_descriptor_reader.m descriptor_model.c probe_model.c -framework IOUSBHost -framework IOKit -framework Foundation -o "$@"

$(PROBE_TEST): tests/probe_model_tests.c probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/probe_model_tests.c probe_model.c -o "$@"

$(OBSERVATION_TEST): tests/observation_model_tests.c observation_model.c observation_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/observation_model_tests.c observation_model.c probe_model.c -o "$@"

$(DESCRIPTOR_TEST): tests/descriptor_model_tests.c descriptor_model.c descriptor_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/descriptor_model_tests.c descriptor_model.c -framework IOUSBHost -o "$@"

test: $(PROBE_TEST) $(OBSERVATION_TEST) $(DESCRIPTOR_TEST)
	"$(PROBE_TEST)"
	"$(OBSERVATION_TEST)"
	"$(DESCRIPTOR_TEST)"

clean:
	rm -rf "$(BUILD_DIR)"
