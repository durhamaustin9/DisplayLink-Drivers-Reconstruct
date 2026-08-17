CC := /usr/bin/xcrun clang
CFLAGS := -std=c11 -Wall -Wextra -Werror -I.
BUILD_DIR := build
PROBE := $(BUILD_DIR)/dock-probe
CHECKER := $(BUILD_DIR)/transcript-check
ACTIVATION_CHECKER := $(BUILD_DIR)/activation-check
DESCRIPTOR_READER := $(BUILD_DIR)/standard-descriptor-reader
FAKE_LAB := $(BUILD_DIR)/fake-dock-lab
EXCHANGE_LAB := $(BUILD_DIR)/exchange-lab
PROTOCOL_MODEL_LAB := $(BUILD_DIR)/protocol-model-lab
PROBE_TEST := $(BUILD_DIR)/probe-model-tests
OBSERVATION_TEST := $(BUILD_DIR)/observation-model-tests
DESCRIPTOR_TEST := $(BUILD_DIR)/descriptor-model-tests
FAKE_TRANSPORT_TEST := $(BUILD_DIR)/fake-transport-tests
STATE_MACHINE_TEST := $(BUILD_DIR)/state-machine-tests
ACTIVATION_TEST := $(BUILD_DIR)/activation-model-tests
EXCHANGE_TEST := $(BUILD_DIR)/exchange-parser-tests
PARTIAL_ORDER_TEST := $(BUILD_DIR)/partial-order-matcher-tests
QUALIFIED_SEQUENCE_TEST := $(BUILD_DIR)/qualified-sequence-tests
TRANSITION_TEST := $(BUILD_DIR)/transition-parser-tests
TRAFFIC_REGIME_TEST := $(BUILD_DIR)/traffic-regime-tests
FUZZ_SMOKE := $(BUILD_DIR)/transition-parser-fuzz-smoke
LIBFUZZER_TARGET := $(BUILD_DIR)/transition-parser-libfuzzer
SANITIZED_DIR := $(BUILD_DIR)/sanitized
SANITIZED_PARTIAL_ORDER_TEST := $(SANITIZED_DIR)/partial-order-matcher-tests
SANITIZED_QUALIFIED_SEQUENCE_TEST := $(SANITIZED_DIR)/qualified-sequence-tests
SANITIZED_TRANSITION_TEST := $(SANITIZED_DIR)/transition-parser-tests
SANITIZED_TRAFFIC_REGIME_TEST := $(SANITIZED_DIR)/traffic-regime-tests
SANITIZED_FUZZ_SMOKE := $(SANITIZED_DIR)/transition-parser-fuzz-smoke
SANITIZED_PROTOCOL_MODEL_LAB := $(SANITIZED_DIR)/protocol-model-lab
SANITIZER_FLAGS := -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined
SANITIZER_ENV := ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
MODEL_MATCHER_SOURCES := partial_order_matcher.c qualified_sequences.c
TRANSITION_SOURCES := transition_parser.c $(MODEL_MATCHER_SOURCES) state_machine.c fake_transport.c transport.c

.PHONY: all probe checker activation-check descriptors fake-lab exchange-lab protocol-model-lab fuzz-smoke fuzz-transition fuzz-libfuzzer check-libfuzzer-runtime test-sanitized test clean

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

exchange-lab: $(EXCHANGE_LAB)

$(EXCHANGE_LAB): exchange_lab.c exchange_parser.c exchange_parser.h fake_transport.c fake_transport.h transport.c transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) exchange_lab.c exchange_parser.c fake_transport.c transport.c -o "$@"

protocol-model-lab: $(PROTOCOL_MODEL_LAB)

$(PROTOCOL_MODEL_LAB): protocol_model_lab.c transition_parser.c transition_parser.h qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h traffic_regime.c traffic_regime.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) protocol_model_lab.c transition_parser.c qualified_sequences.c partial_order_matcher.c traffic_regime.c state_machine.c fake_transport.c transport.c -o "$@"

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

$(EXCHANGE_TEST): tests/exchange_parser_tests.c exchange_parser.c exchange_parser.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/exchange_parser_tests.c exchange_parser.c -o "$@"

$(PARTIAL_ORDER_TEST): tests/partial_order_matcher_tests.c partial_order_matcher.c partial_order_matcher.h protocol_transfer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/partial_order_matcher_tests.c partial_order_matcher.c -o "$@"

$(QUALIFIED_SEQUENCE_TEST): tests/qualified_sequences_tests.c qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/qualified_sequences_tests.c qualified_sequences.c partial_order_matcher.c -o "$@"

$(TRANSITION_TEST): tests/transition_parser_tests.c transition_parser.c transition_parser.h qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/transition_parser_tests.c $(TRANSITION_SOURCES) -o "$@"

$(TRAFFIC_REGIME_TEST): tests/traffic_regime_tests.c traffic_regime.c traffic_regime.h protocol_transfer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/traffic_regime_tests.c traffic_regime.c -o "$@"

$(FUZZ_SMOKE): fuzz/transition_parser_fuzz.c transition_parser.c transition_parser.h qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DDB_FUZZ_STANDALONE fuzz/transition_parser_fuzz.c $(TRANSITION_SOURCES) -o "$@"

fuzz-smoke: $(FUZZ_SMOKE)
	"$(FUZZ_SMOKE)"

fuzz-transition: $(FUZZ_SMOKE)
	@echo "Running 100,000 deterministic metadata-only mutation cases."
	"$(FUZZ_SMOKE)" 100000

check-libfuzzer-runtime:
	@runtime="$$($(CC) -print-file-name=libclang_rt.fuzzer_osx.a)"; \
	if [ "$$runtime" = "libclang_rt.fuzzer_osx.a" ] || [ ! -f "$$runtime" ]; then \
		echo "The selected Clang does not include the optional libFuzzer runtime." >&2; \
		echo "Use 'make fuzz-transition' for the portable deterministic gate, or select a full Xcode toolchain." >&2; \
		exit 69; \
	fi

$(LIBFUZZER_TARGET): check-libfuzzer-runtime Makefile fuzz/transition_parser_fuzz.c transition_parser.c transition_parser.h qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined fuzz/transition_parser_fuzz.c $(TRANSITION_SOURCES) -o "$@"

fuzz-libfuzzer: $(LIBFUZZER_TARGET)
	@echo "Metadata-only libFuzzer; interrupt it when your desired run is complete."
	$(SANITIZER_ENV) "$(LIBFUZZER_TARGET)" -max_len=512 -timeout=2

$(SANITIZED_DIR):
	mkdir -p "$@"

$(SANITIZED_PARTIAL_ORDER_TEST): Makefile tests/partial_order_matcher_tests.c partial_order_matcher.c partial_order_matcher.h protocol_transfer.h | $(SANITIZED_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) tests/partial_order_matcher_tests.c partial_order_matcher.c -o "$@"

$(SANITIZED_QUALIFIED_SEQUENCE_TEST): Makefile tests/qualified_sequences_tests.c qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h | $(SANITIZED_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) tests/qualified_sequences_tests.c qualified_sequences.c partial_order_matcher.c -o "$@"

$(SANITIZED_TRANSITION_TEST): Makefile tests/transition_parser_tests.c transition_parser.c transition_parser.h qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(SANITIZED_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) tests/transition_parser_tests.c $(TRANSITION_SOURCES) -o "$@"

$(SANITIZED_TRAFFIC_REGIME_TEST): Makefile tests/traffic_regime_tests.c traffic_regime.c traffic_regime.h protocol_transfer.h | $(SANITIZED_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) tests/traffic_regime_tests.c traffic_regime.c -o "$@"

$(SANITIZED_FUZZ_SMOKE): Makefile fuzz/transition_parser_fuzz.c transition_parser.c transition_parser.h qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(SANITIZED_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) -DDB_FUZZ_STANDALONE fuzz/transition_parser_fuzz.c $(TRANSITION_SOURCES) -o "$@"

$(SANITIZED_PROTOCOL_MODEL_LAB): Makefile protocol_model_lab.c transition_parser.c transition_parser.h qualified_sequences.c qualified_sequences.h partial_order_matcher.c partial_order_matcher.h protocol_transfer.h traffic_regime.c traffic_regime.h state_machine.c state_machine.h fake_transport.c fake_transport.h transport.c transport.h | $(SANITIZED_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) protocol_model_lab.c transition_parser.c qualified_sequences.c partial_order_matcher.c traffic_regime.c state_machine.c fake_transport.c transport.c -o "$@"

test-sanitized: $(SANITIZED_PARTIAL_ORDER_TEST) $(SANITIZED_QUALIFIED_SEQUENCE_TEST) $(SANITIZED_TRANSITION_TEST) $(SANITIZED_TRAFFIC_REGIME_TEST) $(SANITIZED_FUZZ_SMOKE) $(SANITIZED_PROTOCOL_MODEL_LAB)
	$(SANITIZER_ENV) "$(SANITIZED_PARTIAL_ORDER_TEST)"
	$(SANITIZER_ENV) "$(SANITIZED_QUALIFIED_SEQUENCE_TEST)"
	$(SANITIZER_ENV) "$(SANITIZED_TRANSITION_TEST)"
	$(SANITIZER_ENV) "$(SANITIZED_TRAFFIC_REGIME_TEST)"
	$(SANITIZER_ENV) "$(SANITIZED_FUZZ_SMOKE)"
	$(SANITIZER_ENV) "$(SANITIZED_PROTOCOL_MODEL_LAB)" >/dev/null

test: $(PROBE_TEST) $(OBSERVATION_TEST) $(DESCRIPTOR_TEST) $(FAKE_TRANSPORT_TEST) $(STATE_MACHINE_TEST) $(ACTIVATION_TEST) $(EXCHANGE_TEST) $(PARTIAL_ORDER_TEST) $(QUALIFIED_SEQUENCE_TEST) $(TRANSITION_TEST) $(TRAFFIC_REGIME_TEST) $(FUZZ_SMOKE) $(PROTOCOL_MODEL_LAB)
	"$(PROBE_TEST)"
	"$(OBSERVATION_TEST)"
	"$(DESCRIPTOR_TEST)"
	"$(FAKE_TRANSPORT_TEST)"
	"$(STATE_MACHINE_TEST)"
	"$(ACTIVATION_TEST)"
	"$(EXCHANGE_TEST)"
	"$(PARTIAL_ORDER_TEST)"
	"$(QUALIFIED_SEQUENCE_TEST)"
	"$(TRANSITION_TEST)"
	"$(TRAFFIC_REGIME_TEST)"
	"$(FUZZ_SMOKE)"
	"$(PROTOCOL_MODEL_LAB)" >/dev/null
	PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 tests/test_etw_metadata_import.py
	PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 tests/test_facts_registry.py

clean:
	rm -rf "$(BUILD_DIR)"
