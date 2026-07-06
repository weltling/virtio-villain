# Host side sidecars

A few tests need something to happen on the **host** side while the
guest is running. Hot plug, hot unplug, save and restore, snapshot,
device pause, on the fly config change. Those actions live outside the
guest by definition. The framework supports them through an optional
Python file placed next to the C test, named with the same numeric
ID. When `run` starts a test it looks for `tests/<dir>/<id>*.py`. If
present, the file is imported and its `run(ctx)` function is invoked
on a background thread for the lifetime of that one test.

The sidecar receives a context object with the VMM backend name, the
VMM PID, a private temp directory, the API socket path, the disk path,
a logger, a shared output buffer reader, a stop event, and a
`vm_api(command, args)` helper that talks to `ch-remote` for Cloud
Hypervisor and to QMP for QEMU. The sidecar can wait for guest output
with `ctx.wait_text("[vv]")`, materialize files in `ctx.tmpdir`,
and drive the host with `ctx.vm_api("add-disk", [...])`. The sidecar
exits cleanly when the backend does not support the action or the API
socket is disabled, in which case the guest reports SKIP rather than
FAIL.

## Why this is not the same as VMM integration tests

The VMMs already have integration test suites that cover hot plug,
save and restore, and similar host driven scenarios. Those suites use
the **kernel's** `virtio_pci` and per device drivers to verify the
result. A kernel driver is tolerant by design. It retries, it ignores
optional capabilities, it applies device specific quirks, it accepts a
working subset of registers, and it considers the device healthy as
long as the filesystem mounts and the link passes packets. A whole
class of device side bugs survives that. Malformed capability chains,
wrong `notify_off_multiplier` on a hot added function, a stale
`device_status` carried over from a previous instance, an ignored
`queue_enable`, a `FEATURES_OK` echo that does not round trip, a
broken `device_feature_select` mux on the new device.

Virtio Villain bypasses the kernel driver entirely. The init binary
runs with `initcall_blacklist=virtio_<class>_init`, walks the PCI
capability list itself, derives the doorbell address from
`notify_bar_base + notify_cap.offset + queue_notify_off *
notify_off_multiplier` for the device under test, writes the full reset
and feature negotiation sequence, programs the per queue GPAs by hand,
and asserts on the exact spec mandated values. Sidecar tests apply
that same from scratch driver to a device that **did not exist at
boot**, where the most interesting VMM regressions hide. A sidecar
test answers a different question than an integration test. Integration
asks "does the kernel mount the disk after hot plug". Sidecar asks
"does every spec mandated virtio register on the hot added function
behave correctly when a from scratch driver pokes it".

Sidecars are also the natural place for tests that combine a host
action with a precise guest level assertion the kernel cannot make.
Save and restore that should leave `queue_notify_off` unchanged, hot
unplug followed by hot replug that should leave no residual
`device_status` bits, snapshot during in flight I/O that should drain
the used ring deterministically. Each of those is a single C test
plus a single Python sidecar in the same directory.
