CC := /usr/bin/xcrun clang
CFLAGS := -std=c11 -Wall -Wextra -Werror -I.
BUILD_DIR := build
PROBE := $(BUILD_DIR)/dock-probe
CHECKER := $(BUILD_DIR)/transcript-check
ACTIVATION_CHECKER := $(BUILD_DIR)/activation-check
DESCRIPTOR_READER := $(BUILD_DIR)/standard-descriptor-reader
FAKE_LAB := $(BUILD_DIR)/fake-dock-lab
PROBE_TEST := $(BUILD_DIR)/probe-model-tests
OBSERVATION_TEST := $(BUILD_DIR)/observation-model-tests
DESCRIPTOR_TEST := $(BUILD_DIR)/descriptor-model-tests
FAKE_TRANSPORT_TEST := $(BUILD_DIR)/fake-transport-tests
STATE_MACHINE_TEST := $(BUILD_DIR)/state-machine-tests
ACTIVATION_TEST := $(BUILD_DIR)/activation-model-tests

.PHONY: all probe checker activation-check descriptors fake-lab test clean

all: probe checker

probe: $(PROBE)

$(BUILD_DIR):
	mkdir -p "$@"

$(PROBE): dock_probe.c probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) dock_probe.c probe_model.c -framework IOKit -framework CoreFoundation -o "$@"

checker: $(CHECKER)

$(CHECKER): transcript_check.c observation_model.c observation_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) transcript_check.c observation_model.c probe_model.c -o "$@"

activation-check: $(ACTIVATION_CHECKER)

$(ACTIVATION_CHECKER): activation_check.c activation_model.c activation_model.h activation_replay.c activation_replay.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h observation_model.c observation_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) activation_check.c activation_model.c activation_replay.c state_machine.c fake_transport.c transport.c observation_model.c probe_model.c -o "$@"

descriptors: $(DESCRIPTOR_READER)

$(DESCRIPTOR_READER): standard_descriptor_reader.m descriptor_model.c descriptor_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fobjc-arc standard_descriptor_reader.m descriptor_model.c probe_model.c -framework IOUSBHost -framework IOKit -framework Foundation -o "$@"

fake-lab: $(FAKE_LAB)

$(FAKE_LAB): fake_dock_lab.c state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) fake_dock_lab.c state_machine.c fake_transport.c transport.c -o "$@"

$(PROBE_TEST): tests/probe_model_tests.c probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/probe_model_tests.c probe_model.c -o "$@"

$(OBSERVATION_TEST): tests/observation_model_tests.c observation_model.c observation_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/observation_model_tests.c observation_model.c probe_model.c -o "$@"

$(DESCRIPTOR_TEST): tests/descriptor_model_tests.c descriptor_model.c descriptor_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/descriptor_model_tests.c descriptor_model.c -framework IOUSBHost -o "$@"

$(FAKE_TRANSPORT_TEST): tests/fake_transport_tests.c fake_transport.c fake_transport.h transport.c transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/fake_transport_tests.c fake_transport.c transport.c -o "$@"

$(STATE_MACHINE_TEST): tests/state_machine_tests.c state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/state_machine_tests.c state_machine.c fake_transport.c transport.c -o "$@"

$(ACTIVATION_TEST): tests/activation_model_tests.c activation_model.c activation_model.h activation_replay.c activation_replay.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h observation_model.c observation_model.h probe_model.c probe_model.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/activation_model_tests.c activation_model.c activation_replay.c state_machine.c fake_transport.c transport.c observation_model.c probe_model.c -o "$@"

test: $(PROBE_TEST) $(OBSERVATION_TEST) $(DESCRIPTOR_TEST) $(FAKE_TRANSPORT_TEST) $(STATE_MACHINE_TEST) $(ACTIVATION_TEST)
	"$(PROBE_TEST)"
	"$(OBSERVATION_TEST)"
	"$(DESCRIPTOR_TEST)"
	"$(FAKE_TRANSPORT_TEST)"
	"$(STATE_MACHINE_TEST)"
	"$(ACTIVATION_TEST)"

clean:
	rm -rf "$(BUILD_DIR)"
