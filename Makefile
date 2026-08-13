SHELL := /bin/zsh

PKG ?=
SOURCE_APP ?= $(CURDIR)/.work/source/DisplayLink Manager.app
LOCAL_APP ?= $(CURDIR)/build/DisplayLink Local.app
CORE_APP ?= $(CURDIR)/build/DockBridge Engine.app
CONTROLLER_APP ?= $(CURDIR)/build/DockBridge.app
INSTALLED_CONTROLLER_APP ?= /Applications/DockBridge.app

.PHONY: help test probe descriptors read-descriptors fake-lab test-clean-room prepare local core controller verify verify-local verify-core verify-controller verify-installed integration-local integration-core integration-contained install-contained open-contained tools

help:
	@echo "Independent, read-only hardware work (no DisplayLink software):"
	@echo "  make probe"
	@echo "  make descriptors       # build the opt-in standard descriptor reader"
	@echo "  make read-descriptors  # run it; DockBridge must be completely quit"
	@echo "  make fake-lab          # run the in-memory zero-write state machine"
	@echo "  make test-clean-room"
	@echo
	@echo "Source-only checks:"
	@echo "  make test"
	@echo
	@echo "Legacy proprietary containment research (retained for auditability):"
	@echo "Prepare a pinned vendor input without installing it:"
	@echo '  make prepare PKG="/path/to/DisplayLink...pkg"'
	@echo
	@echo "Build and verify the tested Local profile:"
	@echo '  make integration-local PKG="/path/to/DisplayLink...pkg"'
	@echo
	@echo "Build the Finder-launchable DockBridge compatibility app:"
	@echo '  make integration-contained PKG="/path/to/DisplayLink...pkg"'
	@echo '  make install-contained'
	@echo '  make open-contained'
	@echo
	@echo "Build and verify the experimental Core profile:"
	@echo '  make integration-core PKG="/path/to/DisplayLink...pkg"'

test:
	./scripts/check-repository

probe:
	$(MAKE) -C clean-room probe

descriptors:
	$(MAKE) -C clean-room descriptors

read-descriptors: descriptors
	./clean-room/build/standard-descriptor-reader --read-standard-descriptors

fake-lab:
	$(MAKE) -C clean-room fake-lab
	./clean-room/build/fake-dock-lab

test-clean-room:
	$(MAKE) -C clean-room test

prepare:
	@if [[ -d "$(SOURCE_APP)" ]]; then \
		./scripts/verify-source-app "$(SOURCE_APP)"; \
	elif [[ -n "$(PKG)" ]]; then \
		./scripts/prepare-source "$(PKG)" "$(SOURCE_APP)"; \
	else \
		echo 'PKG is required when no prepared source exists.' >&2; \
		exit 64; \
	fi

local: prepare
	./scripts/build-local-app "$(SOURCE_APP)" "$(LOCAL_APP)"

core: prepare
	./scripts/build-core-app "$(SOURCE_APP)" "$(CORE_APP)"

controller:
	./scripts/verify-core-app "$(CORE_APP)"
	./scripts/build-controller-app "$(CORE_APP)" "$(CONTROLLER_APP)"

verify: verify-local

verify-local:
	./scripts/verify-local-app "$(LOCAL_APP)"

verify-core:
	./scripts/verify-core-app "$(CORE_APP)"

verify-controller:
	./scripts/verify-controller-app "$(CONTROLLER_APP)"

verify-installed:
	./scripts/verify-controller-app "$(INSTALLED_CONTROLLER_APP)"

integration-local: local
	./scripts/verify-local-app "$(LOCAL_APP)"

integration-core: core
	./scripts/verify-core-app "$(CORE_APP)"

integration-contained: integration-core
	./scripts/build-controller-app "$(CORE_APP)" "$(CONTROLLER_APP)"
	./scripts/verify-controller-app "$(CONTROLLER_APP)"

install-contained: verify-controller
	./scripts/install-controller-app "$(CONTROLLER_APP)" "$(INSTALLED_CONTROLLER_APP)"

open-contained: verify-installed
	/usr/bin/open "$(INSTALLED_CONTROLLER_APP)"

tools:
	$(MAKE) -C tools all
