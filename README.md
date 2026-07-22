# Virtio Villain

A systematic test framework that boots a minimal initramfs guest and
exercises the virtio shared memory interface from the guest side,
targeting VMM bugs that arise when the guest violates protocol invariants.

Virtio Villain exercises the host virtio device model with malformed
and out of spec inputs from the guest side, under the adversarial
driver threat model, sometimes called the hostile guest or untrusted
guest model. Each test
is a guest side protocol level fault injection that violates a
driver MUST rule from the spec and asserts the device handles it
without crashing or corrupting state. A subset of tests is
conformance testing in the strict sense, asserting that spec mandated
registers behave correctly. This includes the host side sidecar
tests described later in this document. The framework can also serve
as a base for differential testing by running the same suite against
multiple VMMs and comparing outcomes.

## What It Does

- Proves that a VMM fails to validate guest supplied virtqueue inputs
  (descriptor addresses, lengths, indices, flags, ring entries)
- Triggers logic bugs: infinite loops from descriptor chain cycles,
  panics from out of bounds indexing, assertion failures from violated
  internal invariants
- When combined with ASAN on the VMM: surfaces memory safety issues
  (UAF, heap buffer overflow, double free) that manifest only under
  protocol violations
- Tests the entire device model stack end to end (PCI transport, queue
  handling, device specific request parsing)
- Runs deterministically in CI: same binary, same kernel, same result

## What It Cannot Do

- Prove exploitation - triggering a crash does not demonstrate a
  working exploit
- Cover hypervisor level bugs (KVM/MSHV, VFIO, IOMMU)
- Guarantee absence of bugs - passing means these specific violations
  are handled, untested inputs remain unknown

## Target VMMs

Cloud Hypervisor and QEMU are supported today, both as PCI hosts.
QEMU is also driven in microvm mode for the MMIO transport tests.
The framework is VMM agnostic and any VMM exposing virtio PCI or
MMIO devices can be added by implementing a backend in `run`.

## Architecture

```
virtio-villain/
  Makefile
  run                    # launch VMM, collect results per test
  bin/
    init.c               # harness: boot, discover, run tests, report, exit
  lib/
    virtio_pci.c/.h      # modern virtio PCI transport
    virtio_mmio.c/.h     # virtio MMIO transport (microvm, ARM virt)
    vring.c/.h           # split vring setup and manipulation
    vring_packed.c/.h    # packed virtqueue support (VIRTIO_F_RING_PACKED)
    pci.c/.h             # PCI config space read/write via sysfs
    util.h               # mmap helpers, virt_to_phys, logging
  tests/
    test.h               # test registration macros, result types
    vring/              # T - split vring descriptor and ring violations
    packed/             # P - packed virtqueue violations
    pci/                # PCI - PCI transport specific violations
    mmio/               # M - MMIO transport violations
    state/              # S - state machine and multistep scenarios
    admin/              # A - admin virtqueue commands
    blk/                # B,Z - block and zoned block device violations
    net/                # N - net device violations
    console/            # C - console device violations
    rng/                # RNG - entropy device violations
    vsock/              # V - vsock device violations
    balloon/            # L - memory balloon device violations
    pmem/               # E - persistent memory device violations
    mem/                # R - virtio-mem device violations
    watchdog/           # D - watchdog device violations
    iommu/              # I - virtio-iommu device violations
    rtc/                # RTC - RTC / clock device violations
    fs/                 # F - virtio-fs device violations
```

The test binary is a static C executable that runs as PID 1 inside a
minimal initramfs. It discovers virtio PCI devices, resets and
initializes transport, runs each registered test, and exits with a
pass/fail code.

## Test Coverage

Test cases derived from the virtio specification v1.4 normative
statements (OASIS Committee Specification 01, 8 April 2026). Each
test violates a specific "driver MUST" requirement and checks
whether the VMM handles it safely.

### Transport and cross cutting

| Prefix | Category | Spec | Covers |
|--------|----------|------|--------|
| T   | Transport (split vring) | 2.7 | Descriptor chain manipulation, OOB indices, huge lengths, avail/used ring abuse, notification suppression, notification data, indirect table violations, lifecycle violations, feature negotiation, in order completion |
| P   | Transport (packed vring) | 2.8 | AVAIL/USED flag manipulation, wrap counter confusion, chain length overflow, indirect table abuse, invalid buffer IDs, event suppression, single slot queues, in order packed |
| PCI | PCI transport | 4.1 | Queue configuration violations, MSI-X OOB vectors, notify on disabled queues, cfg_data capability read/write, config generation races, BAR bounds, ISR manipulation, shared memory cap probe, notify multiplier |
| M | MMIO transport | 4.2 | Wrong width register access, undefined offsets, missing QueueSel, reserved status bits, skipped magic check, missing InterruptACK, read only and write only regs, reset, FeaturesSel, QueueNum, ConfigGeneration, SHMRegion access |
| S | State machine / lifecycle | 2.1-3.3 | Multistep negotiation, partial reset recovery, queue reenable without config, split to packed reinit, back to back resets, status bit manipulation, FEATURES_OK rejection, VERSION_1 negotiation, NEEDS_RESET |
| A | Admin commands | 2.13 | Commands without LIST_USE, reserved status codes, truncated payloads, empty group lists, commands after reset, response overflow, zero length commands |

Legacy interface (spec 3.2) is intentionally out of scope. The
framework targets the modern (1.0+) interface only.

### Devices

Status legend: **yes** = tests exist in this framework, **no** = no
tests yet (not implemented in this framework, regardless of whether
the VMM under test exposes the device).

| Prefix | Spec | Device | Tested |
|--------|------|--------|--------|
| N      | 5.1  | Network                              | yes |
| B, Z   | 5.2  | Block (incl. zoned 5.2.6.6)          | yes |
| C      | 5.3  | Console                              | yes |
| RNG    | 5.4  | Entropy (virtio-rng)                 | yes |
| L      | 5.5  | Traditional memory balloon           | yes |
| K      | 5.6  | SCSI host                            | reserved |
| U      | 5.7  | GPU                                  | reserved |
| -      | 5.8  | Input                                | no  |
| -      | 5.9  | Crypto                               | no  |
| V      | 5.10 | Socket (vsock)                       | yes |
| F      | 5.11 | File system (virtio-fs)              | yes |
| -      | 5.12 | RPMB                                 | no  |
| I      | 5.13 | IOMMU                                | yes |
| O      | 5.14 | Sound                                | reserved |
| R      | 5.15 | Memory (virtio-mem)                  | yes |
| H      | 5.16 | I2C                                  | reserved |
| W      | 5.17 | SCMI                                 | reserved |
| Q      | 5.18 | GPIO                                 | reserved |
| E      | 5.19 | Persistent memory (virtio-pmem)      | yes |
| Y      | 5.20 | CAN                                  | reserved |
| -      | 5.21 | SPI Controller                       | no  |
| -      | 5.22 | Media                                | no  |
| RTC    | 5.23 | RTC / Clock                          | yes |
| D      | ID 35 only | Watchdog                       | yes |
| J      | ID 40 only | Bluetooth                      | reserved |
| -      | ID 38 only | Parameter Server               | no  |
| -      | ID 39 only | Audio Policy                   | no  |

Reservation legend: **yes** = tests exist; **reserved** = letter
earmarked for the device, no tests yet; **no** = no letter assigned
yet. The letter `X` is intentionally unused because it collides
visually with MSI-X in PCI capability discussion. All single letters
not listed (currently none) are available for future use.

Spec section numbers above track v1.4 (CS01, 8 April 2026). The
v1.4 Device Types table reserves IDs 1 to 48, but only 23 of them
have a normative chapter (5.1 through 5.23). The rest, including
Watchdog (ID 35), Bluetooth (ID 40), Parameter Server (ID 38),
Audio Policy (ID 39), plus several others (9P, pstore, GPU video
codecs, NitroSecureModule, RDMA, Camera, ISM, TEE, CPU balloon),
are allocated for interoperability but their spec text lives in
separate OASIS drafts or has not landed yet. virtio-villain tests
for such devices follow the de facto behavior of the VMM under
test.

### Out of scope

The following surfaces are deliberately not covered by virtio-villain:

- **Legacy virtio (1.0-)** - spec 3.2 - modern only focus
- **VFIO PCI passthrough** - kernel framework, the guest talks to
  real hardware via KVM rather than to a VMM emulated device
- **vfio-user** - userspace device emulation over a UNIX socket.
  A separate protocol that would need a different harness because
  the VMM is the *client* of the socket, not the server

## Building

```bash
make            # produces ./init (static binary, all tests linked in)
make initramfs  # packages init into initramfs.cpio.gz
```

The initramfs contains the single `init` binary with all tests compiled
in. The test to run is selected via kernel command line:

- `vv.test=T01` - run a single test
- `vv.test=all` - run all tests sequentially (useful when the VMM is
  not expected to crash)
- `vv.test=list` - print all available test IDs and exit

Since a successful test may crash the VMM, `run` spawns a fresh VM
per test and collects results across invocations. For MMIO transport
tests (M prefix), use `--mmio` with a QEMU binary to run against the
microvm machine type.

List available tests on the host (output adapts to terminal width,
truncating long descriptions with `~`):

```bash
./init --list
```

Columns: test ID, description, spec version, spec section.

The binary is built with musl and stripped for the initramfs. Each test
adds ~200-500 bytes of code, so even with 300+ tests the image stays
under 4MB. For debugging, keep the unstripped binary on the host and
use `addr2line` against VMM crash addresses.

## Test Results

Each test reports one of five outcomes:

- **PASS** - the device consumed the request and advanced the used ring.
  For malformed inputs this means the VMM processed it without crashing
- **FAIL** - the device did something detectably wrong. Only emitted by
  tests that carry custom validation logic
- **REJECT** - the device correctly stayed silent and did not advance
  the used ring but remains alive and responsive. The ideal outcome
  for invalid inputs that should be silently dropped
- **WEDGED** - the device stopped responding and is no longer healthy.
  device_status reports DEVICE_NEEDS_RESET or zero, or subsequent
  well formed requests are ignored. Typically the VMM queue worker
  thread died in response to the malformed input. Recovery requires
  tearing the device down and re creating it via driver unbind and
  rebind, or hot unplug and replug. A plain virtio reset through
  device_status is not guaranteed to resurrect a thread that has
  already exited
- **SKIP** - the required device is not present or a precondition
  cannot be met, for example packed queues not offered

PASS and REJECT are acceptable outcomes. WEDGED indicates the VMM
handled the violation ungracefully and the device is unusable from
that point on without operator intervention. FAIL indicates a VMM bug
such as a crash or core dump. The `run` script exits nonzero if
there are any FAILs or WEDGEDs.

With `--retries N` a failing test is rerun. A test that then reaches an
acceptable verdict is reported under its real verdict (PASS, REJECT or
SKIP) and listed at the end under RETRIED with the sequence of attempt
verdicts, for example `B0042 [FAIL, PASS]`. A retried result does not
fail the run. A test that fails every retry keeps its failing verdict
and still gates.

A retried result is not a claim that the test is inherently unreliable.
Under heavy parallelism or batching a test can fail once for reasons of
the run rather than the test, for example a CPU starved guest missing
its timeout, or a batch neighbor wedging the shared VM. The solo rerun
removes the batch neighbor and often the load, so the retry mainly
keeps the exit code honest. To confirm a test is genuinely
nondeterministic, run it isolated and serialized with `--jobs 1
--batch 0 --retries N`.

## Host side sidecars

A few tests need a host side action while the guest runs: hot plug,
hot unplug, save and restore, snapshot, device pause, or an on the fly
config change. These are driven by an optional Python file placed next
to the C test with the same numeric ID, whose `run(ctx)` runs for the
lifetime of that test and drives the host through `ch-remote` or QMP.
This exercises spec mandated registers on a device that did not exist
at boot, which a tolerant kernel driver never checks. See
[docs/sidecars.md](docs/sidecars.md).

## Quick Start

```bash
make initramfs

./run -m ./cloud-hypervisor                  # all tests
./run -m ./cloud-hypervisor T01              # single test
./run -m ./cloud-hypervisor T01 T03 B01      # subset
./run -m ./cloud-hypervisor -j4              # 4 parallel VMs
```

`-m` takes a path to the VMM binary. You build or install it
yourself. The kernel is auto fetched into `target/` if `-k` is
omitted, which uses the Cloud Hypervisor guest kernel from GitHub
releases. Pass `-k /path/to/vmlinux` to use your own.

Output is colored when writing to a terminal:

- $\textcolor{green}{\textsf{PASS}}$ green
- $\textcolor{red}{\textsf{FAIL}}$ red
- $\textcolor{magenta}{\textsf{REJECT}}$ magenta
- $\textcolor{orange}{\textsf{WEDGED}}$ yellow
- $\textcolor{cyan}{\textsf{SKIP}}$ cyan

A progress line at the bottom shows current test and running totals.
Pipe to a file or through `| cat` to suppress colors.

### Common variations

```bash
./run -m ./cloud-hypervisor -k /path/to/vmlinux            # custom kernel
./run -m ./cloud-hypervisor -d qcow2                       # use qcow2 backing
./run -m ./cloud-hypervisor M01                            # MMIO test, auto launches QEMU microvm if needed
./run -m ./qemu-system-x86_64                              # QEMU backend
./run -m ./cloud-hypervisor -v T01                         # verbose, full test output
./run -m ./cloud-hypervisor -c T01                         # forward guest console to stdout
./run -m ./cloud-hypervisor --retries 4 T01               # retry a failure up to 4 times, list it retried if it recovers
```

For correctness runs keep `--jobs` modest. Heavy oversubscription
(many parallel VMs each with several vCPUs on fewer host cores) starves
the guests of CPU time and can make a stable test fail once. The runner
already scales the per test timeout by the oversubscription factor, but
timing sensitive tests are best confirmed at a low `--jobs`. Use
`--retries N` to tell a real regression apart from an oversubscription
artifact. Only failing tests are retried, so a green run pays no extra
cost.

### Machine readable output

For CI, the runner can emit a machine readable report instead of (or
alongside) the human table:

```bash
./run -m ./cloud-hypervisor --format json                  # JSON report on stdout
./run -m ./cloud-hypervisor --format junit                 # JUnit XML on stdout
./run -m ./cloud-hypervisor --format junit --output r.xml  # JUnit XML to a file
```

With `--format json` or `--format junit` the human table moves to
stderr and the report goes to stdout, so `2>/dev/null` leaves only the
report. With `--output FILE` the report is written to the file and the
human table stays on stdout. The JUnit mapping is: FAIL, WEDGED and
XPASS become `<failure>`, SKIP and XFAIL become `<skipped>`, PASS and
REJECT are plain test cases. The exit code is unchanged: nonzero when
any test fails, wedges, or xpasses.

Every failing verdict also carries a security severity tier that scores
the VMM behaviour by attacker capability: CRITICAL for guest controlled
memory corruption in the host VMM (the precursor to a guest to host
escape), HIGH for a VMM crash, panic or hang, LOW for a device that
wedges itself with NEEDS_RESET while the VMM keeps running, and NONE for
a graceful rejection. The tier appears in the FAIL and WEDGED detail
lines of the human output, as a `security` object per test in the JSON
report, and as a `security` attribute plus an inline note on the JUnit
`<failure>` node. The tier flags only that a fault is reachable;
exploitability always needs manual analysis. The same triage drives the
`run-fuzz replay` and `run-fuzz triage` crash corpus tooling.

### Dependencies

Build:

- A C compiler capable of producing a static x86_64 binary (gcc,
  clang, or musl-gcc all work)
- `make`
- `cpio`, `gzip` (for initramfs target)

Run:

- Python 3 (no pip dependencies)

Fuzzing (optional, only needed for `run-fuzz minimize` and `cov-report`):

- `llvm-profdata` and `llvm-cov`, matching the toolchain used to
  build the coverage instrumented VMM

### VMM Setup

The test framework runs against any VMM that exposes virtio PCI devices.
The only thing you must provide is the VMM binary itself, passed to
`run` with `-m`. For deeper bug coverage, build it with ASAN (Rust
VMMs: `RUSTFLAGS="-Zsanitizer=address"`, C/C++ VMMs:
`-fsanitize=address`).

Everything else `run` arranges automatically on first invocation:

- Guest kernel. If `--kernel` is not given, `run` downloads
  `vmlinux-x86_64` from the latest Cloud Hypervisor kernel release
  and caches it under `target/`. To use your own kernel, pass
  `--kernel /path/to/vmlinux` and make sure it is booted with
  `initcall_blacklist=virtio_blk_init` so that the guest does not
  claim the virtio block device before the test harness. The `run`
  script appends this argument for you.
- Initramfs. Built once by `make initramfs` and reused across tests.
- Per test backing files. A fresh disk image, pmem region, and where
  applicable a vsock socket and virtiofs share are created in a
  temporary directory for each test, then removed. Disk format is
  selectable with `--disk-format` and defaults to raw, raw images
  use `truncate`, qcow2, vhd and vhdx use `qemu-img` so install
  `qemu-utils` if you want non raw formats.
- Optional helpers. `virtiofsd` is launched automatically for the
  virtio-fs tests if it is on `PATH`, otherwise those tests are
  skipped.

To wire all of this up by hand, for example when integrating into a
custom harness, the equivalent steps are: build the VMM, fetch a
kernel, run `make initramfs` once, allocate a 16 MiB disk image and a
128 MiB pmem file per test, and pass kernel, initramfs, disk, pmem
and the `vv.test=NAME` cmdline argument to the VMM. Reading
`run.build_cmd` for the backend you target is the shortest path to
the exact command line.

## Fuzzing

Alongside the deterministic suite, a coverage guided mutation fuzzer
encodes a descriptor chain into a 4096 byte blob, patches it into a
fuzz guest, boots the VMM, and classifies the result. With a coverage
instrumented VMM it feeds new edges back into the corpus, and crashes
are grouped by error class for triage. See
[docs/fuzzing.md](docs/fuzzing.md).

## Found Bugs

### Cloud Hypervisor

A subset of upstream Cloud Hypervisor fixes surfaced by this harness:

- [#8491](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8491) virtio-devices: Omit the device config capability for configless devices
- [#8388](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8388) block: Fix WriteZeroes sector arithmetic overflow
- [#8295](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8295) virtio-devices: Signal NEEDS_RESET
- [#8272](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8272) virtio-devices: Respect PCI CFG cap.length for BAR access
- [#8238](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8238) virtio-devices: Fix cap_len for VIRTIO_PCI_CAP_PCI_CFG
- [#8232](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8232) vm-virtio: Add centralized descriptor range validation
- [#8190](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8190) virtio-devices: Require at least one ready queue for activation
- [#8185](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8185) virtio-devices: Bump config_generation on device specific config reads
- [#8177](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8177) virtio-devices: balloon: Cap inflate and deflate descriptor length
- [#8166](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8166) virtio-devices: net, block: Drop per handler NEEDS_RESET bookkeeping
- [#8142](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8142) net: Prevent worker thread death on malformed guest descriptors
- [#8138](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8138) virtio-devices: Check MSI-X vector bounds before table access
- [#8131](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/8131) pci: msix: Replace panic with graceful error on invalid table write
- [#7949](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/7949) virtio-devices: block: Fix writeback mode update flow
- [#7921](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/7921) virtio-devices: block: Use error specific status in sync fallback path
- [#7858](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/7858) virtio-devices: block: Derive discard alignment from topology
- [#7852](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/7852) virtio-devices: block: Populate discard and write zeroes config
- [#7805](https://github.com/cloud-hypervisor/cloud-hypervisor/pull/7805) virtio: Spec compliance fixes

All in cloud-hypervisor/cloud-hypervisor, all merged.

### QEMU

- [#3968](https://gitlab.com/qemu-project/qemu/-/work_items/3968) virtio: Malformed packed descriptor livelocks the device at 100% host CPU (guest triggered DoS), reproducer P0040

## Acknowledgments

Test development is accelerated with GitHub Copilot, which assists in
generating spec driven fault injection cases, scaffolding new device
categories, and maintaining coverage across the virtio specification
surface.

## Maintainer

Anatol Belski <anbelski@linux.microsoft.com>

This is a personal open source project. It is not affiliated with,
endorsed by, or sponsored by any employer, and contributions and
releases are not made on behalf of any organization.

## License

Apache-2.0
