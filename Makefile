# SPDX-License-Identifier: Apache-2.0

ifneq ($(strip $(SUITE)),)
.DEFAULT_GOAL := initramfs
include lib/suite.mk
.PHONY: help
help:
	@echo "Suite targets: init, initramfs (default), kernel, prepare"
	@echo "Set TARGET to select the output root relative to the suite directory."
	@echo "Compiler and suite build variables can be passed as Make overrides."
else

ifeq ($(origin CC),default)
CC = musl-gcc
endif
CC      ?= musl-gcc
CFLAGS  ?= -O2 -Wall -Wextra -Werror -Wformat=2 \
           -Wstrict-prototypes -Wmissing-prototypes \
           -Wvla -Wshadow -Wundef -Wwrite-strings -Wpointer-arith \
           -Werror=implicit-function-declaration \
           -Werror=incompatible-pointer-types \
           -fstack-protector-strong -fstack-clash-protection
LDFLAGS ?= -Wl,-z,noexecstack
CSTD     = -std=gnu23
STATIC_LDFLAGS ?= -static
STRIP   ?= strip
TARGET  ?= target
OBJDIR  ?= $(TARGET)/obj
SELFTEST_DIR ?= $(TARGET)/selftest

# Auto generate per object header dependencies.
DEPFLAGS = -MMD -MP

# Quiet by default; pass V=1 for full command lines.
V ?= 0
Q = $(if $(filter 1,$(V)),,@)

# Write a value to a file only when it changed, so that targets that
# depend on the file rebuild only on a real change.
update_file = printf '%s\n' '$(subst ','\'',$(2))' | cmp -s - $(1) || \
              printf '%s\n' '$(subst ','\'',$(2))' > $(1)

# Make only compares timestamps. Record the toolchain and flags, so that
# changing them rebuilds everything instead of mixing objects built by
# different compilers.
BUILD_FLAGS = $(CC) $(CPPFLAGS) $(CSTD) $(CFLAGS) $(LDFLAGS) $(STATIC_LDFLAGS)

%/.flags: FORCE
	@mkdir -p $(@D)
	@$(call update_file,$@,$(BUILD_FLAGS))

# Harness version that init prints at boot, so that every test log names
# the exact commit. Outside a Git checkout, pass VV_VERSION=... instead.
VV_VERSION := $(shell git describe --always --dirty 2>/dev/null || \
                echo unknown)

SRCS = bin/init.c \
       lib/pci.c \
       lib/irq.c \
       lib/virtio_pci.c \
       lib/virtio_mmio.c \
       lib/vring.c \
       lib/vring_packed.c \
       $(wildcard tests/*/*.c) \
       $(wildcard tests/*/fuzz/*.c)

OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

$(TARGET)/init: $(OBJS) $(OBJDIR)/.flags
	@mkdir -p $(TARGET)
	@echo "  LINK  $@"
	$(Q)$(CC) $(CSTD) $(CFLAGS) $(LDFLAGS) $(STATIC_LDFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: %.c $(OBJDIR)/.flags
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	$(Q)$(CC) $(CPPFLAGS) $(CSTD) $(CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

# Only init.o gets the version, so that a new commit does not rebuild all
# objects. "private" keeps the define away from the prerequisites, and
# "override" keeps it when CPPFLAGS is set on the command line.
$(OBJDIR)/bin/init.o: private override CPPFLAGS += \
                      -DVV_VERSION='"$(VV_VERSION)"'
$(OBJDIR)/bin/init.o: $(OBJDIR)/.version

$(OBJDIR)/.version: FORCE
	@mkdir -p $(@D)
	@$(call update_file,$@,$(VV_VERSION))

-include $(DEPS)

initramfs: $(TARGET)/init
	$(Q)python3 lib/python/villain/initramfs.py --init "$<" --output "$(TARGET)/initramfs.cpio.gz" --strip "$(STRIP)"
	@echo "$(TARGET)/initramfs.cpio.gz ($$(du -h $(TARGET)/initramfs.cpio.gz | cut -f1))"

deps-check:
	@command -v $(CC) >/dev/null 2>&1 || { echo "error: $(CC) not found. Install musl-tools: apt install musl-tools"; exit 1; }
	@command -v $(STRIP) >/dev/null 2>&1 || { echo "error: $(STRIP) not found. Install binutils: apt install binutils"; exit 1; }
	@command -v python3 >/dev/null 2>&1 || { echo "error: python3 not found. apt install python3"; exit 1; }
	@echo "deps ok: $(CC), strip, python3"
	@if command -v virtiofsd >/dev/null 2>&1 \
	    || [ -x /usr/libexec/virtiofsd ] \
	    || [ -x /usr/lib/virtiofsd ] \
	    || [ -x /usr/lib/qemu/virtiofsd ]; then \
	    echo "optional: virtiofsd found, virtio-fs (F*) tests enabled"; \
	else \
	    echo "optional: virtiofsd not found, virtio-fs (F*) tests will SKIP"; \
	    echo "          install with: apt install virtiofsd"; \
	fi

fuzz-deps-check: deps-check
	@command -v llvm-profdata >/dev/null 2>&1 || { echo "error: llvm-profdata not found. apt install llvm"; exit 1; }
	@command -v llvm-cov >/dev/null 2>&1 || { echo "error: llvm-cov not found. apt install llvm"; exit 1; }
	@echo "fuzz deps ok: llvm-profdata, llvm-cov"

FUZZ_SRCS = bin/fuzz.c \
            lib/pci.c \
            lib/virtio_pci.c \
            lib/vring.c

FUZZ_OBJS = $(patsubst %.c,$(OBJDIR)/fuzz/%.o,$(FUZZ_SRCS))
FUZZ_DEPS = $(FUZZ_OBJS:.o=.d)

$(TARGET)/fuzz: $(FUZZ_OBJS) $(OBJDIR)/.flags
	@mkdir -p $(TARGET)
	@echo "  LINK  $@"
	$(Q)$(CC) $(CSTD) $(CFLAGS) $(LDFLAGS) $(STATIC_LDFLAGS) -o $@ $(FUZZ_OBJS)

$(OBJDIR)/fuzz/%.o: %.c $(OBJDIR)/.flags
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	$(Q)$(CC) $(CPPFLAGS) $(CSTD) $(CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

-include $(FUZZ_DEPS)

PERF_SRCS = bin/perf.c \
            lib/pci.c \
            lib/perf_engine.c \
            lib/virtio_pci.c \
            lib/vring.c \
            lib/vring_packed.c

PERF_OBJS = $(patsubst %.c,$(OBJDIR)/perf/%.o,$(PERF_SRCS))
PERF_DEPS = $(PERF_OBJS:.o=.d)

$(TARGET)/perf: $(PERF_OBJS) $(OBJDIR)/.flags
	@mkdir -p $(TARGET)
	@echo "  LINK  $@"
	$(Q)$(CC) $(CSTD) $(CFLAGS) $(LDFLAGS) $(STATIC_LDFLAGS) -o $@ $(PERF_OBJS)

$(OBJDIR)/perf/%.o: %.c $(OBJDIR)/.flags
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	$(Q)$(CC) $(CPPFLAGS) $(CSTD) $(CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

-include $(PERF_DEPS)

# External appliances supply an explicit source list; never extend virtio SRCS.
APPLIANCE_SRCS ?=
APPLIANCE_TARGET ?= $(TARGET)/appliance
APPLIANCE_CPPFLAGS ?=
APPLIANCE_ALL_SRCS = $(APPLIANCE_SRCS) lib/test_runner.c lib/appliance_init.c
APPLIANCE_OBJS = $(patsubst %.c,$(APPLIANCE_TARGET)/obj/%.o,$(APPLIANCE_ALL_SRCS))

appliance-check:
	@test -n "$(APPLIANCE_SRCS)" || { echo "error: set APPLIANCE_SRCS explicitly"; exit 1; }

$(APPLIANCE_OBJS): | appliance-check

# APPLIANCE_CPPFLAGS changes the appliance objects too.
$(APPLIANCE_TARGET)/.flags: BUILD_FLAGS += $(APPLIANCE_CPPFLAGS)

$(APPLIANCE_TARGET)/obj/%.o: %.c $(APPLIANCE_TARGET)/.flags
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CPPFLAGS) $(CSTD) $(CFLAGS) $(DEPFLAGS) -I. $(APPLIANCE_CPPFLAGS) -c -o $@ $<

$(APPLIANCE_TARGET)/init: $(APPLIANCE_OBJS) $(APPLIANCE_TARGET)/.flags appliance-check
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(CSTD) $(CFLAGS) $(LDFLAGS) $(STATIC_LDFLAGS) -o $@ $(APPLIANCE_OBJS)

appliance: $(APPLIANCE_TARGET)/init

appliance-initramfs: $(APPLIANCE_TARGET)/init
	$(Q)python3 lib/python/villain/initramfs.py --init "$<" --output "$(APPLIANCE_TARGET)/initramfs.cpio.gz"

-include $(APPLIANCE_OBJS:.o=.d)

fuzz-initramfs: $(TARGET)/fuzz
	$(Q)python3 lib/python/villain/initramfs.py --init "$<" --output "$(TARGET)/fuzz-initramfs.cpio.gz" --strip "$(STRIP)"
	@echo "$(TARGET)/fuzz-initramfs.cpio.gz ($$(du -h $(TARGET)/fuzz-initramfs.cpio.gz | cut -f1))"

perf-initramfs: $(TARGET)/perf
	$(Q)python3 lib/python/villain/initramfs.py --init "$<" --output "$(TARGET)/perf-initramfs.cpio.gz" --strip "$(STRIP)"
	@echo "$(TARGET)/perf-initramfs.cpio.gz ($$(du -h $(TARGET)/perf-initramfs.cpio.gz | cut -f1))"

clean:
	$(Q)python3 lib/python/villain/clean.py --root "$(CURDIR)" \
	  "$(TARGET)" "$(OBJDIR)" "$(APPLIANCE_TARGET)" \
	  selftest/test_lib selftest/test_irq selftest/cloud-hypervisor

selftest-c: $(SELFTEST_DIR)/test_lib $(SELFTEST_DIR)/test_irq
	@$(SELFTEST_DIR)/test_lib
	@$(SELFTEST_DIR)/test_irq

selftest-sanitize:
	$(MAKE) selftest-c SELFTEST_DIR=$(TARGET)/selftest-sanitize \
	  CFLAGS="$(CFLAGS) -O1 -g -fno-omit-frame-pointer \
	    -fsanitize=address,undefined -fno-sanitize-recover=all" \
	  LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined"

selftest: selftest-c
	@python3 -m unittest discover -s selftest -p 'test_msix_host.py'
	@python3 -m unittest discover -s selftest -p 'test_kernel.py'
	@python3 -m unittest discover -s selftest -p 'test_artifacts.py'
	@python3 -m unittest discover -s selftest -p 'test_build.py'
	@python3 -m unittest discover -s selftest -p 'test_appliance.py'
	@python3 -m unittest discover -s selftest -p 'test_reporting.py'
	@python3 -m unittest discover -s selftest -p 'test_runner_modules.py'
	@python3 -m unittest discover -s selftest -p 'test_clean.py'
	@echo ""
	@echo "selftest/run:"
	@python3 selftest/test_run.py
	@echo ""
	@echo "selftest/perf:"
	@python3 selftest/test_perf.py

# Any header change rebuilds the selftests. They build in under a second,
# so this is simpler than exact header lists, which can go stale.
SELFTEST_HDRS = $(wildcard lib/*.h tests/*.h)

$(SELFTEST_DIR)/test_lib: selftest/test_lib.c lib/perf_engine.c lib/vring.c \
                          lib/vring_packed.c $(SELFTEST_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CSTD) $(CFLAGS) -I. -o $@ $(filter %.c,$^) $(LDFLAGS)

$(SELFTEST_DIR)/test_irq: selftest/test_irq.c lib/irq.c $(SELFTEST_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CSTD) $(CFLAGS) -I. -o $@ $(filter %.c,$^) $(LDFLAGS)

$(SELFTEST_DIR)/test_lib $(SELFTEST_DIR)/test_irq: $(SELFTEST_DIR)/.flags

PY_SRCS = run run-fuzz run-perf build-kernel $(shell find selftest tests lib -name '*.py' 2>/dev/null)

flake8-deps-check:
	@command -v flake8 >/dev/null 2>&1 || python3 -m flake8 --version >/dev/null 2>&1 || { echo "error: flake8 not found. apt install flake8 or pip install flake8"; exit 1; }

flake8: flake8-deps-check
	@if command -v flake8 >/dev/null 2>&1; then \
	    flake8 $(PY_SRCS); \
	else \
	    python3 -m flake8 $(PY_SRCS); \
	fi

init: $(TARGET)/init
fuzz: $(TARGET)/fuzz
perf: $(TARGET)/perf

KERNEL_CONFIGS ?=
KERNEL_ARGS ?=

guest-kernel:
	./build-kernel $(foreach config,$(KERNEL_CONFIGS),--config "$(config)") $(KERNEL_ARGS)

help:
	@echo "Targets:"
	@echo "  init                Build the guest init binary at $(TARGET)/init"
	@echo "  initramfs           Pack init into $(TARGET)/initramfs.cpio.gz"
	@echo "  fuzz                Build the fuzz guest at $(TARGET)/fuzz"
	@echo "  fuzz-initramfs      Pack fuzz into $(TARGET)/fuzz-initramfs.cpio.gz"
	@echo "  perf                Build the performance guest at $(TARGET)/perf"
	@echo "  perf-initramfs      Pack perf into $(TARGET)/perf-initramfs.cpio.gz"
	@echo "  appliance           Build explicit APPLIANCE_SRCS independently"
	@echo "  appliance-initramfs Pack the external appliance as /init"
	@echo "  selftest            Build and run runner and lib self tests"
	@echo "  selftest-c          Build and run C library selftests"
	@echo "  selftest-sanitize   Run C selftests with ASan and UBSan (CC=gcc/clang)"
	@echo "  guest-kernel        Build an optional Linux image with KERNEL_ARGS"
	@echo "  flake8              Run flake8 across run, run-fuzz, selftest, tests"
	@echo "  deps-check          Verify build dependencies are present"
	@echo "  fuzz-deps-check     Verify fuzz build dependencies are present"
	@echo "  flake8-deps-check   Verify flake8 is present"
	@echo "  clean               Remove build outputs"
	@echo "  help                Show this message"
	@echo ""
	@echo "Build variables:"
	@echo "  TARGET              Output directory (default: target)"
	@echo "  OBJDIR              Object directory (default: TARGET/obj)"
	@echo "  CC, CFLAGS, LDFLAGS  C compiler and build flags"
	@echo "  STRIP               Guest executable stripping tool"
	@echo "  V=1                 Print compiler commands"
	@echo "  APPLIANCE_SRCS      Explicit external appliance C source list"
	@echo "  APPLIANCE_TARGET    External output directory (default: TARGET/appliance)"
	@echo "  APPLIANCE_CPPFLAGS  External appliance include paths and compiler flags"
	@echo "  KERNEL_CONFIGS      Ordered config files or fragments for guest-kernel"
	@echo "                      Later files override earlier values"
	@echo "                      Empty by default, using the bundled kernel profile"
	@echo "  KERNEL_ARGS         Extra build-kernel options, processed after KERNEL_CONFIGS"
	@echo "                      Custom configs require --output DIR"
	@echo "  RUSTC               Optional environment selection for the kernel Rust compiler"
	@echo ""
	@echo "Examples:"
	@echo '  make guest-kernel KERNEL_CONFIGS="/path/base.config /path/extra.config" KERNEL_ARGS="--source /path/linux --output /path/kernel-build"'
	@echo "  ./build-kernel --help lists all kernel source and build options"
	@echo "  make SUITE=/path/to/suite initramfs builds an external suite"
	@echo ""
	@echo "clean removes TARGET, custom object/appliance outputs and host test binaries."
	@echo "Caches and results inside those output directories are removed too."

FORCE:

.PHONY: FORCE init fuzz perf initramfs fuzz-initramfs perf-initramfs clean selftest selftest-c selftest-sanitize flake8 flake8-deps-check help guest-kernel
.PHONY: appliance appliance-initramfs appliance-check
endif
