# SPDX-License-Identifier: Apache-2.0

CC      ?= musl-gcc
CFLAGS  ?= -static -O2 -Wall -Wextra -Werror
LDFLAGS ?= -static
TARGET  ?= target
OBJDIR  ?= $(TARGET)/obj

# Auto generate per object header dependencies.
DEPFLAGS = -MMD -MP

# Quiet by default; pass V=1 for full command lines.
V ?= 0
Q = $(if $(filter 1,$(V)),,@)

SRCS = bin/init.c \
       lib/pci.c \
       lib/virtio_pci.c \
       lib/virtio_mmio.c \
       lib/vring.c \
       lib/vring_packed.c \
       $(wildcard tests/*/*.c) \
       $(wildcard tests/*/fuzz/*.c)

OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

$(TARGET)/init: $(OBJS)
	@mkdir -p $(TARGET)
	@echo "  LINK  $@"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -I. -o $@ $(OBJS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	$(Q)$(CC) $(CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

-include $(DEPS)

initramfs: $(TARGET)/init
	@rm -rf $(TARGET)/.rootfs
	@mkdir -p $(TARGET)/.rootfs/proc $(TARGET)/.rootfs/sys $(TARGET)/.rootfs/dev
	@cp $(TARGET)/init $(TARGET)/.rootfs/init
	@strip $(TARGET)/.rootfs/init
	@cd $(TARGET)/.rootfs && find . | cpio -o -H newc 2>/dev/null | gzip > ../initramfs.cpio.gz
	@rm -rf $(TARGET)/.rootfs
	@echo "$(TARGET)/initramfs.cpio.gz ($$(du -h $(TARGET)/initramfs.cpio.gz | cut -f1))"

deps-check:
	@command -v $(CC) >/dev/null 2>&1 || { echo "error: $(CC) not found. Install musl-tools: apt install musl-tools"; exit 1; }
	@command -v strip >/dev/null 2>&1 || { echo "error: strip not found. Install binutils: apt install binutils"; exit 1; }
	@command -v cpio >/dev/null 2>&1 || { echo "error: cpio not found. apt install cpio"; exit 1; }
	@command -v gzip >/dev/null 2>&1 || { echo "error: gzip not found. apt install gzip"; exit 1; }
	@command -v python3 >/dev/null 2>&1 || { echo "error: python3 not found. apt install python3"; exit 1; }
	@echo "deps ok: $(CC), strip, cpio, gzip, python3"
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

$(TARGET)/fuzz: $(FUZZ_OBJS)
	@mkdir -p $(TARGET)
	@echo "  LINK  $@"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -I. -o $@ $(FUZZ_OBJS)

$(OBJDIR)/fuzz/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	$(Q)$(CC) $(CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

-include $(FUZZ_DEPS)

PERF_SRCS = bin/perf.c \
            lib/pci.c \
            lib/virtio_pci.c \
            lib/vring.c

PERF_OBJS = $(patsubst %.c,$(OBJDIR)/perf/%.o,$(PERF_SRCS))
PERF_DEPS = $(PERF_OBJS:.o=.d)

$(TARGET)/perf: $(PERF_OBJS)
	@mkdir -p $(TARGET)
	@echo "  LINK  $@"
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -I. -o $@ $(PERF_OBJS)

$(OBJDIR)/perf/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	$(Q)$(CC) $(CFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

-include $(PERF_DEPS)

fuzz-initramfs: $(TARGET)/fuzz
	@rm -rf $(TARGET)/.fuzzfs
	@mkdir -p $(TARGET)/.fuzzfs/proc $(TARGET)/.fuzzfs/sys $(TARGET)/.fuzzfs/dev
	@cp $(TARGET)/fuzz $(TARGET)/.fuzzfs/init
	@strip $(TARGET)/.fuzzfs/init
	@cd $(TARGET)/.fuzzfs && find . | cpio -o -H newc 2>/dev/null | gzip > ../fuzz-initramfs.cpio.gz
	@rm -rf $(TARGET)/.fuzzfs
	@echo "$(TARGET)/fuzz-initramfs.cpio.gz ($$(du -h $(TARGET)/fuzz-initramfs.cpio.gz | cut -f1))"

perf-initramfs: $(TARGET)/perf
	@rm -rf $(TARGET)/.perffs
	@mkdir -p $(TARGET)/.perffs/proc $(TARGET)/.perffs/sys $(TARGET)/.perffs/dev
	@cp $(TARGET)/perf $(TARGET)/.perffs/init
	@strip $(TARGET)/.perffs/init
	@cd $(TARGET)/.perffs && find . | cpio -o -H newc 2>/dev/null | gzip > ../perf-initramfs.cpio.gz
	@rm -rf $(TARGET)/.perffs
	@echo "$(TARGET)/perf-initramfs.cpio.gz ($$(du -h $(TARGET)/perf-initramfs.cpio.gz | cut -f1))"

clean:
	rm -rf $(TARGET) selftest/test_lib selftest/cloud-hypervisor

selftest: selftest/test_lib
	@echo "selftest/lib:"
	@./selftest/test_lib
	@echo ""
	@echo "selftest/run:"
	@python3 selftest/test_run.py

selftest/test_lib: selftest/test_lib.c lib/vring.c lib/vring.h tests/test.h
	$(CC) -O2 -Wall -Wextra -I. -o $@ selftest/test_lib.c lib/vring.c

PY_SRCS = run run-fuzz $(shell find selftest tests -name '*.py' 2>/dev/null)

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

help:
	@echo "Targets:"
	@echo "  init                Build the guest init binary at $(TARGET)/init"
	@echo "  initramfs           Pack init into $(TARGET)/initramfs.cpio.gz"
	@echo "  fuzz                Build the fuzz guest at $(TARGET)/fuzz"
	@echo "  fuzz-initramfs      Pack fuzz into $(TARGET)/fuzz-initramfs.cpio.gz"
	@echo "  perf                Build the performance guest at $(TARGET)/perf"
	@echo "  perf-initramfs      Pack perf into $(TARGET)/perf-initramfs.cpio.gz"
	@echo "  selftest            Build and run runner and lib self tests"
	@echo "  flake8              Run flake8 across run, run-fuzz, selftest, tests"
	@echo "  deps-check          Verify build dependencies are present"
	@echo "  fuzz-deps-check     Verify fuzz build dependencies are present"
	@echo "  flake8-deps-check   Verify flake8 is present"
	@echo "  clean               Remove build outputs"
	@echo "  help                Show this message"

.PHONY: init fuzz perf initramfs fuzz-initramfs perf-initramfs clean selftest flake8 flake8-deps-check help
