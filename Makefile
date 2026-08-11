SHELL := /bin/zsh

PKG ?=
SOURCE_APP ?= $(CURDIR)/.work/source/DisplayLink Manager.app
LOCAL_APP ?= $(CURDIR)/build/DisplayLink Local.app
CORE_APP ?= $(CURDIR)/build/DisplayLink Core.app

.PHONY: help test prepare local core verify verify-local verify-core integration-local integration-core tools

help:
	@echo "Source-only checks:"
	@echo "  make test"
	@echo
	@echo "Prepare a pinned vendor input without installing it:"
	@echo '  make prepare PKG="/path/to/DisplayLink...pkg"'
	@echo
	@echo "Build and verify the tested Local profile:"
	@echo '  make integration-local PKG="/path/to/DisplayLink...pkg"'
	@echo
	@echo "Build and verify the experimental Core profile:"
	@echo '  make integration-core PKG="/path/to/DisplayLink...pkg"'

test:
	./scripts/check-repository

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

verify: verify-local

verify-local:
	./scripts/verify-local-app "$(LOCAL_APP)"

verify-core:
	./scripts/verify-core-app "$(CORE_APP)"

integration-local: local
	./scripts/verify-local-app "$(LOCAL_APP)"

integration-core: core
	./scripts/verify-core-app "$(CORE_APP)"

tools:
	$(MAKE) -C tools all
