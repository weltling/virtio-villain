#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

if [ "$#" -eq 0 ]; then
    echo "usage: sh selftest/test_mmio_codegen.sh <ARM64 compiler> [flags...]" >&2
    exit 1
fi

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
assembly=$(mktemp)
trap 'rm -f "$assembly"' EXIT
trap 'exit 1' HUP INT TERM

"$@" -std=gnu23 -O2 -Wall -Wextra -Werror -ffreestanding -S \
    "$root/selftest/test_mmio_codegen.c" -o "$assembly"

awk '
BEGIN {
    expected["probe_read8"] = "ldrb"
    expected["probe_read16"] = "ldrh"
    expected["probe_read32"] = "ldr"
    expected["probe_read64"] = "ldr"
    expected["probe_write8"] = "strb"
    expected["probe_write16"] = "strh"
    expected["probe_write32"] = "str"
    expected["probe_write64"] = "str"
    expected["probe_read_bytes"] = "ldrb"
}
/^probe_[a-z0-9_]+:/ {
    name = $0
    sub(/:.*/, "", name)
}
/^[[:space:]]*\.size[[:space:]]/ { name = "" }
name in expected && /^[[:space:]]*(ld|st)[a-z0-9]*[[:space:]]/ {
    line = $0
    sub(/\/\/.*/, "", line)
    # The byte copy destination is ordinary RAM and may use writeback.
    if (name == "probe_read_bytes" && line ~ /^[[:space:]]*strb[[:space:]]/)
        next
    pattern = "^[[:space:]]*" expected[name] \
              "[[:space:]]+w[0-9]+,[[:space:]]*\\[x[0-9]+\\][[:space:]]*$"
    if (line !~ pattern) {
        print "FAIL " name ": " $0 > "/dev/stderr"
        failed = 1
    }
    count[name]++
}
END {
    for (name in expected) {
        want = name ~ /64$/ ? 2 : 1
        if ((name == "probe_read_bytes" && count[name] < 1) ||
            (name != "probe_read_bytes" && count[name] != want)) {
            print "FAIL " name ": unexpected access count " \
                  count[name] > "/dev/stderr"
            failed = 1
        }
    }
    if (failed)
        exit 1
    print "MMIO ARM64 code generation checks passed"
}
' "$assembly"
