SHELL := /bin/zsh
BUILD_DIR := build
CFLAGS := -std=c11 -Wall -Wextra -Werror

.PHONY: all syntax

all: $(BUILD_DIR)/display-status $(BUILD_DIR)/network-probe $(BUILD_DIR)/macho-payload $(BUILD_DIR)/rename-excl

syntax:
	clang $(CFLAGS) -Wno-deprecated-declarations -fsyntax-only display-status.c
	clang $(CFLAGS) -fsyntax-only network-probe.c
	clang $(CFLAGS) -fsyntax-only macho-payload.c
	clang $(CFLAGS) -fsyntax-only rename-excl.c

$(BUILD_DIR):
	mkdir -p "$(BUILD_DIR)"

$(BUILD_DIR)/display-status: display-status.c | $(BUILD_DIR)
	clang $(CFLAGS) -Wno-deprecated-declarations \
		-framework ApplicationServices -framework IOKit \
		display-status.c -o "$@"

$(BUILD_DIR)/network-probe: network-probe.c | $(BUILD_DIR)
	clang $(CFLAGS) network-probe.c -o "$@"

$(BUILD_DIR)/macho-payload: macho-payload.c | $(BUILD_DIR)
	clang $(CFLAGS) macho-payload.c -o "$@"

$(BUILD_DIR)/rename-excl: rename-excl.c | $(BUILD_DIR)
	clang $(CFLAGS) rename-excl.c -o "$@"
