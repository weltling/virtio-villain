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

The default output reports the average time for one request across all measured
rounds. It also shows the fastest and slowest round averages so run stability is
visible. Use `--verbose` to print the elapsed time and average request time for
every round. JSON output keeps all samples for automated comparison.

This workload has queue depth one, so only one request is active at a time.
Average request latency is therefore the measured time divided by the request
count. It is not a latency percentile.

JSON output includes every sample and the host, VMM, and run settings. A VMM
version is omitted when the binary has no supported version query.

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
workload. It is not a replacement for storage benchmarks such as fio. Packed
queues, request batches, writes, indirect descriptors, and multiple queues
are candidates for additional workloads.