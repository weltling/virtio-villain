# Performance

`run-perf` measures valid virtio requests separately from conformance and
robustness verdicts. It boots one guest, runs a warmup, then records several
rounds without including VM startup in the samples.

Each workload submits one request at a time through a split virtqueue. Block
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

```bash
./run-perf -m ./cloud-hypervisor --warmup 5000 --rounds 10 -n 50000
./run-perf -m ./cloud-hypervisor --io-engine io_uring --direct
```

## Results

The default output reports mean service time across all measured rounds. It
also shows the fastest and slowest round means so run stability is visible.
Use `--verbose` to print elapsed time and mean service time for every round.
JSON output keeps all samples for automated comparison.

This workload has queue depth one, so only one request is active at a time.
Mean service time is the measured round duration divided by the request count.
It is not a latency sample or percentile.

JSON schema version 3 separates experiment, host, guest, VMM, execution, queue,
workload, backend, and instrumentation settings. Each sample records request
bytes, submissions, completions, notifications, timing mode, and clock source.
A VMM version is omitted when the binary has no supported version query.

The current queue depth and batch size are one. The guest submits, completes,
and notifies once for each request. Vsock uses one request queue operation and
one response queue operation for each measured transaction. It uses one more
request queue operation to reset the completed connection.

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

The runner does not assign pass or fail thresholds. Performance depends on
host load, CPU placement, VMM build settings, block backend settings, and
instrumentation. Compare results only when these inputs match.

Coverage and sanitizer instrumentation add substantial cost. Use a release
VMM build without instrumentation for representative measurements. Keep the
host load stable and run enough rounds to expose variance.

## Scope

This runner measures serial request processing in the current split queue
workload over PCI. It is not a replacement for storage benchmarks such as fio.
Packed queues, request batches, writes, indirect descriptors, and multiple
queues are candidates for additional workloads.
