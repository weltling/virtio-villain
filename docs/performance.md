# Performance

`run-perf` measures valid virtio requests separately from conformance and
robustness verdicts. It boots one guest, runs a warmup, then records several
rounds without including VM startup in the samples.

Each workload submits concurrent requests through a split virtqueue. Block
reads 4 KiB. RNG fills 4 KiB. Network transmits a 64 byte Ethernet frame.
Vsock sends a connection request and consumes the response. The guest owns
the virtqueues directly without a guest filesystem or kernel device driver.

## Usage

```bash
./run-perf -m ./cloud-hypervisor --device blk
./run-perf -m ./qemu-system-x86_64 --device blk
```

`--device` selects the virtio device under measurement. Supported workloads
are `blk`, `rng`, `net`, and `vsock`. OpenVMM does not provide a vsock device.

```bash
./run-perf -m ./cloud-hypervisor --device rng
./run-perf -m ./qemu-system-x86_64 --device net
./run-perf -m ./cloud-hypervisor --device vsock
```

The default run uses 1000 warmup requests followed by five measured rounds
of 10000 requests. The guest uses one virtual CPU, one block queue, and 256
MiB of memory.

Use `--iterations`, `--warmup`, and `--rounds` to change the sample shape.
Use `--cpus`, `--memory`, `--io-engine`, and `--direct` to change the VM and
block backend configuration.

Use `--queue-depth` to select a power of two depth from 1 through 16. Use
`--batch-size` to publish 1, 4, 8, or 16 descriptor heads before each device
notification. Batch size must not exceed queue depth. Throughput is the only
current timing mode.

```bash
./run-perf -m ./cloud-hypervisor --warmup 5000 --rounds 10 -n 50000
./run-perf -m ./cloud-hypervisor --io-engine io_uring --direct
```

## Results

The default output reports completed operations per second across all measured
rounds. It also shows the fastest and slowest round rates so run stability is
visible. Mean service time is derived from the duration of the complete round.
It is not a latency sample or percentile. Use `--verbose` to print elapsed time
and operation rate for every round. JSON output keeps all samples for automated
comparison.

Block and RNG reports include payload bytes per second. Network transmit only
proves that the device consumed the buffer, so it does not report delivered
payload rate. Vsock also omits payload rate. Every workload reports device
notifications per submission.

JSON schema version 3 separates experiment, host, guest, VMM, execution, queue,
workload, backend, instrumentation, and timing settings. Each sample records
request bytes, submissions, completions, notifications, timing mode, and clock
source. Throughput rounds use `CLOCK_MONOTONIC_RAW` with one read at each round
boundary. A VMM version is omitted when the binary has no supported version
query.

Queue depth and batch size default to one. Vsock uses one request queue
operation and one response queue operation for each measured transaction. It
uses one more request queue operation to reset the completed connection. Exact
submission, completion, and notification counts include all three operations.

Use `--experiment-class` and `--changed-dimension` to identify the one layer
changed by a comparison. The available experiment classes are `queue`,
`frontend`, and `backend`.

```bash
./run-perf -m ./openvmm --device blk \
	--experiment-class queue --changed-dimension none
```

```bash
./run-perf -m ./cloud-hypervisor --format json
./run-perf -m ./cloud-hypervisor --format json -o perf.json
```

Compare two compatible JSON reports without starting a VMM.

```bash
./run-perf --compare baseline.json candidate.json
./run-perf --compare baseline.json candidate.json --format json
```

Comparison reports show the combined operation rate change, each observed
round range, relative spread, and whether the ranges overlap. They do not
assign a regression verdict or apply a fixed threshold. Reports with different
fixed inputs are rejected before a performance result is produced. The one
dimension named by the experiment class may differ.

The runner does not assign pass or fail thresholds. Performance depends on
host load, CPU placement, VMM build settings, block backend settings, and
instrumentation. Compare results only when these inputs match.

Coverage and sanitizer instrumentation add substantial cost. Use a release
VMM build without instrumentation for representative measurements. Keep the
host load stable and run enough rounds to expose variance.

## Scope

This runner measures request throughput in the current split queue workload
over PCI. It is not a replacement for storage benchmarks such as fio. Packed
queues, writes, indirect descriptors, and multiple device queues are candidates
for additional work.
