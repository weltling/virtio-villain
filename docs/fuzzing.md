# Fuzzing

In addition to the deterministic test suite, the repository includes a
coverage guided mutation fuzzer that generates structured virtqueue inputs
and boots them against a VMM to find crashes.

## Components

- `bin/fuzz.c` - minimal guest (PID 1) that reads a version 1 blob
  from its `.fuzz_input` ELF section, builds each encoded vring, kicks
  the queues, and reboots
- `lib/fuzz_input.h` - 64 KiB blob format with a versioned container
  and up to 16 device queue segments. Each segment contains a packed
  queue header, descriptor records, avail ring entries, and payload
  bytes
- `run-fuzz` - Python3 orchestrator that mutates blobs, patches them
  into the fuzz guest ELF, boots the VMM, and classifies results

## Usage

```bash
make fuzz            # build the fuzz guest ELF
make fuzz-initramfs  # package into initramfs

./run-fuzz fuzz --vmm ./cloud-hypervisor                  # auto downloads kernel into target/
./run-fuzz fuzz --vmm ./cloud-hypervisor --kernel path/to/vmlinux
./run-fuzz fuzz --vmm ./cloud-hypervisor -n 1000          # 1000 iterations
./run-fuzz fuzz --vmm ./cloud-hypervisor -j 8 --cpus 1    # 8 parallel VMs
./run-fuzz fuzz --vmm ./cloud-hypervisor --no-coverage    # skip llvm-cov
```

Defaults are 10000 iterations, 1 job, 2 vCPUs, 128M of guest RAM, and a
3 second VMM boot timeout. Corpus inputs land under
`target/corpus/DEVICE/` and crash blobs under `target/crashes/DEVICE/`.
Both directory trees are gitignored and persist across runs.

Other subcommands.

```bash
./run-fuzz seed                                                # seed corpus from existing tests
./run-fuzz decode target/crashes/blk/crash_*.bin               # print blob contents
./run-fuzz replay --vmm ./cloud-hypervisor target/crashes/...  # reproduce one or more crashes
./run-fuzz triage --vmm ./cloud-hypervisor                     # group crashes by error class
./run-fuzz minimize --vmm ./cloud-hypervisor                   # drop corpus entries with redundant coverage
./run-fuzz cov-report --vmm ./cloud-hypervisor                 # summarize edges hit by the corpus
```

`minimize` and `cov-report` need a coverage instrumented VMM and the
`llvm-profdata` and `llvm-cov` tools listed under Dependencies.

## Mutation Strategies

`run-fuzz` picks one strategy per iteration, uniformly at random,
from the following set. The blob format (`lib/fuzz_input.h`) starts
with a versioned container header. Each segment identifies a device
and queue, followed by a queue header, descriptor table, avail ring,
and payload area.

- `bit_flip`, `byte_arith`, `interesting_16` - generic byte and word
  level mutations
- `endian_swap`, `zero_fill_region`, `payload_noise` - structural
  noise on payload regions
- `header_corrupt`, `queue_size_mutate` - rewrite the blob header so
  the guest builds a malformed queue
- `desc_flags`, `desc_addr_mutate`, `grow_desc`, `shrink_desc`,
  `duplicate_desc`, `chain_shuffle` - mutate the descriptor table
  (flags, addresses, lengths, ordering, multiplicity)
- `avail_corrupt`, `avail_ring_replay` - corrupt or replay avail
  ring entries
- `indirect_inject` - turn a regular descriptor into an indirect
  table reference
- `splice_corpus` - splice bytes from another corpus blob into the
  current one
- `multi_strategy` - apply 2 or 3 of the above in sequence

## Coverage Guidance

When the VMM is built with coverage instrumentation, `run-fuzz`
collects `llvm-profdata` / `llvm-cov` output after each run to
identify inputs that reach new code paths. These are added to the
corpus for further mutation. Without instrumentation, the fuzzer
operates in blind mutation mode.

### Building Cloud Hypervisor for Fuzzing

Coverage only with the stable toolchain.

```bash
cd cloud-hypervisor
RUSTFLAGS="-C instrument-coverage" cargo build
```

AddressSanitizer only on nightly. Catches memory safety bugs in
unsafe code, UAF, OOB, and use after poison.

```bash
RUSTFLAGS="-Zsanitizer=address" \
  cargo +nightly build -Zbuild-std --target x86_64-unknown-linux-gnu
```

Coverage instrumentation and AddressSanitizer are independent LLVM
passes with separate runtimes, so a single binary can carry both. The
combined build needs nightly and build-std (because ASan does), and is
heavier and slower than either alone. This is the only build that gives
`run-fuzz` both growing coverage and memory error detection at once.

```bash
RUSTFLAGS="-C instrument-coverage -Zsanitizer=address" \
  cargo +nightly build -Zbuild-std --target x86_64-unknown-linux-gnu
```

Separate single signal builds are still recommended for routine
campaigns: they build faster, run faster, and keep each signal clean.
`run-fuzz` reads coverage from the source based `instrument-coverage`
profraw via `llvm-profdata` and `llvm-cov`, so a coverage signal needs
that flag specifically.

Ensure `rust-src` is available.

```bash
rustup component add rust-src --toolchain nightly-x86_64-unknown-linux-gnu
```

| Build | What it catches | Overhead |
|-------|----------------|----------|
| `-C instrument-coverage` | panics, asserts, logic bugs | ~3x |
| `-Zsanitizer=address` | memory safety in unsafe blocks | ~10x |

The `run-fuzz` script automatically passes `--seccomp false` to the
VMM since coverage profiling emits files at exit which requires
syscalls not in the default seccomp allowlist.

Ensure `llvm-profdata` and `llvm-cov` match the LLVM version used by
`rustc`.

```bash
rustup component add llvm-tools-preview
```

Or use the system LLVM if versions match.

## Triaging Crashes

Each file under `target/crashes/DEVICE/` is a version 1 fuzz blob. To
investigate:

```bash
./run-fuzz decode target/crashes/blk/crash_*.bin                              # print blob contents
./run-fuzz replay --vmm ./cloud-hypervisor target/crashes/blk/crash_XXXX.bin  # reproduce
./run-fuzz triage --vmm ./cloud-hypervisor                                # group all by error class
```

`decode` prints each device queue segment, queue config, descriptor
chain, avail ring entries, and payload stats for each blob.

`replay` patches each blob into the fuzz guest, boots the VMM, and
reports whether it crashed or exited cleanly. Use `--timeout 10` for
slow VMMs.

`triage` replays every blob under the crash directory tree and groups
them by the panic message or signal observed, which collapses dozens
of inputs that hit the same root cause into one bucket.

After fixing a VMM bug, replay all crashes to confirm which ones no
longer reproduce, many blobs often trigger the same underlying issue.
