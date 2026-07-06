# Fuzzing

In addition to the deterministic test suite, the repository includes a
coverage guided mutation fuzzer that generates random virtqueue inputs
and boots them against a VMM to find crashes.

## Components

- `bin/fuzz.c` - minimal guest (PID 1) that reads a 4096-byte blob
  from its `.fuzz_input` ELF section, builds a vring from the encoded
  descriptor chain, kicks the queue, and reboots
- `lib/fuzz_input.h` - blob format: packed header (queue_size,
  num_descs, avail_idx, avail_count), descriptor structs (len, flags,
  next), avail ring entries, and raw payload bytes
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

Defaults are 10000 iterations, 1 job, 1 vCPU, 128M of guest RAM, and a
3 second VMM boot timeout. Corpus inputs land in `target/corpus/` and
crash blobs in `target/crashes/`. Both directories are gitignored and
persist across runs.

Other subcommands.

```bash
./run-fuzz seed                                                # seed corpus from existing tests
./run-fuzz decode target/crashes/crash_*.bin                   # print blob contents
./run-fuzz replay --vmm ./cloud-hypervisor target/crashes/...  # reproduce one or more crashes
./run-fuzz triage --vmm ./cloud-hypervisor                     # group crashes by error class
./run-fuzz minimize --vmm ./cloud-hypervisor                   # drop corpus entries with redundant coverage
./run-fuzz cov-report --vmm ./cloud-hypervisor                 # summarize edges hit by the corpus
```

`minimize` and `cov-report` need a coverage instrumented VMM and the
`llvm-profdata` and `llvm-cov` tools listed under Dependencies.

## Mutation Strategies

`run-fuzz` picks one strategy per iteration, uniformly at random,
from the following set. The blob format (`lib/fuzz_input.h`) is a
4096 byte record with a header, a descriptor table, an avail ring,
and a raw payload area.

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

Each file in `target/crashes/` is a raw 4096 byte blob. To investigate:

```bash
./run-fuzz decode target/crashes/crash_*.bin                              # print blob contents
./run-fuzz replay --vmm ./cloud-hypervisor target/crashes/crash_XXXX.bin  # reproduce
./run-fuzz triage --vmm ./cloud-hypervisor                                # group all by error class
```

`decode` prints the queue config, descriptor chain (lengths, flags,
next pointers), avail ring entries, and payload stats for each blob.

`replay` patches each blob into the fuzz guest, boots the VMM, and
reports whether it crashed or exited cleanly. Use `--timeout 10` for
slow VMMs.

`triage` replays every blob in `target/crashes/` and groups them by
the panic message or signal observed, which collapses dozens of
inputs that hit the same root cause into one bucket.

After fixing a VMM bug, replay all crashes to confirm which ones no
longer reproduce, many blobs often trigger the same underlying issue.
