# Performance

`run-perf` measures valid virtio requests separately from conformance and
robustness verdicts. It boots one guest, runs a warmup, then records several
rounds without including VM startup in the samples.

Each workload submits concurrent requests through a split virtqueue. Block
reads or writes 4 KiB. RNG fills 4 KiB. Network transmits a 64 byte Ethernet
frame or receives a runner supplied 64 byte Ethernet frame. Vsock sends a
connection request and consumes the response. The guest owns the virtqueues
directly without a guest filesystem or kernel device driver.

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

Network requests default to transmit. Use `--net-operation receive` to post
writable receive buffers and measure frames delivered into guest memory.

```bash
./run-perf -m ./qemu-system-x86_64 --device net \
	--net-operation receive --queue-depth 16 --batch-size 16
./run-perf -m ./openvmm --device net \
	--net-operation receive --queue-depth 16 --batch-size 16
./run-perf -m ./cloud-hypervisor --device net \
	--net-operation receive --queue-depth 16 --batch-size 16
```

For QEMU, the runner sends fixed 64 byte Ethernet frames through a UDP socket
backend. For OpenVMM, it sends a fixed payload through Consomme UDP forwarding.
For Cloud Hypervisor, it sends the payload through a named TAP interface. The
Cloud Hypervisor binary must have effective `cap_net_admin`. The guest validates
the complete frame before counting an operation. Receive reports include
delivered payload bytes per second.

The default run uses 1000 warmup requests followed by five measured rounds
of 10000 requests. The guest uses one virtual CPU, one block queue, and 256
MiB of memory.

Use `--iterations`, `--warmup`, and `--rounds` to change the sample shape.
Use `--cpus`, `--memory`, `--io-engine`, and `--direct` to change the VM and
block backend configuration.

Use `--disk-type` to select `raw`, `qcow2`, `vhd`, or `vhdx`. Raw is the
default. Other formats require `qemu-img`. The selected format is recorded in
the backend report section.

Block requests default to reads at sector zero. Use `--block-operation` to
select `read` or `write`. Use `--block-pattern` to select `fixed`,
`sequential`, or `random` addresses. Use `--block-request-size` to select
512, 1024, 2048, or 4096 bytes.

```bash
./run-perf -m ./cloud-hypervisor --device blk \
	--block-operation write --block-pattern sequential \
	--block-request-size 1024
./run-perf -m ./qemu-system-x86_64 --device blk \
	--block-operation read --block-pattern random
```

Requests transfer 4 KiB by default. Fixed requests use sector zero. Sequential
requests advance by the selected request size and wrap at device capacity.
Random requests use a fixed deterministic sequence and stay within device
capacity. Write buffers contain a fixed byte value. Write completion is
measured at the used ring and does not imply that data reached durable storage.

The current request sizes fit in one guest page. Larger requests require a
descriptor segment layout that can represent separate guest physical pages.

Use `--queue-depth` to select a power of two depth from 1 through 32. Use
`--batch-size` to publish 1, 4, 8, 16, or 32 descriptor heads before each device
notification. Batch size must not exceed queue depth. Throughput is the default
timing mode.

Use `--device-queues` to distribute block requests across 1, 2, 4, 8, or 16
request queues. Queue depth applies to each device queue. The guest publishes
one wave across all active queues before it waits for completions. A partial
final wave fills queues in queue index order.

```bash
./run-perf -m ./cloud-hypervisor --device blk \
	--device-queues 4 --queue-depth 16 --batch-size 4
```

Multiple device queues require the block MQ feature. The runner configures the
VMM with the requested block queue count and raises the virtual CPU count to
the same value when needed. The guest rejects a device that does not offer MQ
or exposes fewer queues than requested. Network multiqueue needs control queue
activation and a suitable host endpoint, so this option is block only.
QEMU and Cloud Hypervisor expose block MQ. The current OpenVMM block device
does not offer MQ and the run reports `device_queues_unsupported`.

Use `--descriptor-layout indirect` to place each request chain in an indirect
descriptor table. The split ring then contains one outer descriptor for each
request. Direct descriptors remain the default.

```bash
./run-perf -m ./openvmm --device blk \
	--descriptor-layout indirect --queue-depth 16
```

Indirect mode requires the indirect descriptor feature. The guest rejects a
device that does not offer the feature, and each result records the negotiated
feature mask. Compare direct and indirect reports as a queue experiment with
`descriptor_layout` as the changed dimension.

Use `--queue-format packed` to submit the same workload through a packed
virtqueue. Split queues remain the default. Queue depth, batch size, descriptor
layout, workload, and timing controls have the same meaning in both formats.

```bash
./run-perf -m ./openvmm --device blk \
	--queue-format packed --queue-depth 16 --batch-size 4
```

Packed mode requires packed ring feature bit 34. The guest rejects a device
that does not offer the feature. QEMU block, network, and RNG complete in
packed mode. OpenVMM block also completes. The current Cloud Hypervisor block
device and QEMU vsock device do not offer packed queues. Compare split and
packed reports as a queue experiment with `format` as the changed dimension.

Use `--notification-policy event_idx` to negotiate the event index feature.
The guest reads the device supplied available event value after each batch and
notifies the device only when the event threshold is crossed. The default
`always` policy notifies the device after every published batch.

```bash
./run-perf -m ./openvmm --device blk \
	--notification-policy event_idx --queue-depth 16 --batch-size 1
```

Event index mode requires feature bit 29. Notification counts record the kicks
that the guest sends, so a device can suppress some or all batch notifications.
Compare the policies as a queue experiment with `notification_policy` as the
changed dimension. Event index notification policy currently requires a split
queue.

```bash
./run-perf -m ./cloud-hypervisor --warmup 5000 --rounds 10 -n 50000
./run-perf -m ./cloud-hypervisor --io-engine io_uring --direct
```

Use latency mode to sample operation latency with RDTSCP. `--sample-every N`
selects every Nth operation and defaults to 10.

```bash
./run-perf -m ./cloud-hypervisor --device blk \
	--timing-mode latency --sample-every 10
```

The sample begins immediately before its descriptor batch is published. It
ends after the used entry for the selected descriptor is observed. Selected
operations in the same batch share one start timestamp. Vsock starts at
connection request publication and ends when the matching response is
observed.

The guest calibrates the TSC against `CLOCK_MONOTONIC_RAW` before device setup.
Latency mode requires RDTSCP. A missing RDTSCP capability or CPU migration
during a sample rejects the run. Keep one virtual CPU for the most stable
latency measurements. QEMU latency runs use its `host` CPU model so RDTSCP is
visible to the guest. The selected CPU model is stored in the report.

## Results

The default output reports completed operations per second across all measured
rounds. It also reports the median round rate and median absolute deviation in
operations per second and as a percentage of the median. These values describe
rounds within one guest boot. The fastest and slowest round rates remain visible.
Mean service time is derived from the duration of the complete round. It is not
a latency sample or percentile. Use `--verbose` to print elapsed time and
operation rate for every round. JSON output keeps all samples for automated
comparison.

Block, RNG, and network receive reports include payload bytes per second.
Block reports also record and display the read or write operation and the
fixed, sequential, or random address pattern. Comparison output displays these
fields for both reports. Network transmit only proves that the device consumed
the buffer, so it does not report delivered payload rate. Vsock also omits
payload rate. Every workload reports device notifications per submission.

JSON schema version 3 separates experiment, host, guest, VMM, execution, queue,
workload, backend, instrumentation, and timing settings. Each sample records
request bytes, submissions, completions, notifications, timing mode, and clock
source. Queue settings record the device queue count and depth per queue.
Each execution records a UUID that identifies its independent guest boot.
The execution section also records Unix nanosecond markers immediately before
and after the complete VMM command.
The queue settings also record the split or packed format and the direct or
indirect descriptor layout. They record the notification policy and negotiated
feature mask as well.
Throughput rounds use `CLOCK_MONOTONIC_RAW` with one read at each round
boundary. Latency rounds store the sampling interval and every raw sample in
nanoseconds. The runner calculates p50, p90, p99, and p99.9 with the nearest
rank method across all measured rounds. A VMM version is omitted when the
binary has no supported version query.
The VMM section records the SHA256 of the executable bytes.

Use `--strace-profile PATH` to wrap only the VMM process with `strace -f -c`.
The raw syscall count and time summary is written to `PATH`. The absolute
artifact path is recorded in the report instrumentation section.

```bash
./run-perf -m ./openvmm --device blk --format json -o perf.json \
	--strace-profile perf.strace
```

System call tracing adds measurement cost. Use it to locate host side time,
not as an uninstrumented throughput result. Apply the same tracing setting to
both VMMs when comparing traced reports.

Latency mode still reports complete round duration and operation rate for
context. Its instrumentation cost means those rates are not throughput mode
results. Throughput mode does not make percentile claims.

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

## Host preparation

Inspect the CPU topology before choosing a CPU set. Keep all logical siblings
of each selected physical core in the set. Avoid CPUs that handle storage,
network, or other frequent interrupts. On a host with multiple NUMA nodes, use
CPUs and memory from the node local to the device under test.

```bash
lscpu -e=CPU,NODE,SOCKET,CORE,ONLINE
cat /proc/interrupts
```

Set `CPU_LIST` to the selected comma separated logical CPU list. Apply that
affinity to the runner so the VMM inherits it.

```bash
taskset -c "$CPU_LIST" ./run-perf ...
```

Inspect the frequency driver, governor, energy preference, and turbo state.
Record the original values before changing them. When the driver provides a
performance governor, use it for the complete campaign. Keep the turbo state
unchanged between baseline and candidate runs.

```bash
cpupower frequency-info
cat /sys/devices/system/cpu/cpufreq/policy*/scaling_governor
```

If `irqbalance` is active, stop it after selecting CPUs so it cannot move
interrupts during the campaign. Record its original state and restore that
state afterward.

Store performance images on the same filesystem and storage device for every
run. Close active builds, editor indexers, other VMs, and graphical remote
sessions before a campaign. Run from SSH or a text console when possible. Wait
until the one minute load average is near zero.

```bash
uptime
```

Run one workload at a time. Do not run baseline and candidate VMMs at the same
time. For two VMM binaries, alternate independent boots in this order.

```text
baseline
candidate
candidate
baseline
```

Repeat this sequence at least three times for six independent boots per VMM.
Use the median and median absolute deviation across boots for the comparison.
The statistics in one report cover rounds within one boot and do not replace
this boot level summary.

Use at least 10000 warmup requests, 100000 requests per round, and five rounds
when comparing block backends.

```bash
taskset -c "$CPU_LIST" ./run-perf -m ./openvmm --device blk \
	--warmup 10000 --rounds 5 -n 100000
```

Keep the request shape, image format, queue settings, direct IO setting, and
VMM build type identical across a comparison. The runner creates a fresh image
for each boot.

Use an exit trap in campaign scripts to restore every changed governor, energy
preference, turbo setting, and service state after success, failure, or an
interruption. Do not assume the original governor or service state.

## Scope

This runner measures request throughput or sampled request latency in the
current virtqueue workload over PCI. It is not a replacement for storage
benchmarks such as fio. Network receive uses a runner supplied packet source
and measures delivery into guest memory. Network multiqueue is a candidate for
additional work.
