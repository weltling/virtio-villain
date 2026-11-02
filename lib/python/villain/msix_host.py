# SPDX-License-Identifier: Apache-2.0
"""Checked host synchronization for MSI-X mapping tests."""

import json
import os
import subprocess

from villain.sidecar_helpers import ch_command, qmp


def check_disk(ctx):
    if not ctx.disk.endswith(".raw"):
        ctx.skip("MSI-X lifecycle synchronization requires a raw disk")


def wait_phase(ctx, marker):
    if ctx.wait_text(marker, timeout=20.0):
        return True
    if ctx.stop.is_set():
        return False
    raise RuntimeError(f"Guest did not reach {marker}")


def write_token(ctx, token):
    data = token.encode("ascii").ljust(512, b"\0")
    if len(data) != 512:
        raise ValueError("MSI-X host token exceeds one sector")
    with open(ctx.disk, "r+b") as stream:
        if stream.write(data) != len(data):
            raise OSError("Short MSI-X disk token write")
        stream.flush()
        os.fsync(stream.fileno())


def disk_capacity(ctx):
    if ctx.disk.endswith(".raw"):
        return os.path.getsize(ctx.disk)
    result = subprocess.run(
        ["qemu-img", "info", "-U", "--output=json", ctx.disk],
        capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"qemu-img info failed: {result.stderr}")
    size = json.loads(result.stdout)["virtual-size"]
    if type(size) is not int or size <= 0:
        raise RuntimeError(f"Invalid virtual disk capacity: {size!r}")
    return size


def monitor(ctx, command):
    reply = qmp(ctx, "human-monitor-command", {"command-line": command})
    if reply["return"] != "":
        raise RuntimeError(f"QEMU human-monitor-command {command} failed: "
                           f"{reply['return']!r}")


def restore_mapping(ctx):
    if ctx.backend.name == "openvmm":
        ch_command(ctx, "pulse-save-restore")
        write_token(ctx, "vv-msix-restored")
        ctx.log("OpenVMM device state saved and restored")
        return
    if ctx.backend.name == "ch":
        snap_dir = os.path.join(ctx.tmpdir, "msix-snapshot")
        os.makedirs(snap_dir, exist_ok=True)
        ch_command(ctx, "pause")
        try:
            ch_command(ctx, "snapshot", [f"file://{snap_dir}"])
        finally:
            ch_command(ctx, "resume")
    else:
        overlay = os.path.join(ctx.tmpdir, "msix-snapshot.qcow2")
        qmp(ctx, "stop")
        try:
            qmp(ctx, "blockdev-snapshot-sync", {
                "device": "vd0", "snapshot-file": overlay, "format": "qcow2",
            })
            monitor(ctx, "savevm vv-msix")
        finally:
            qmp(ctx, "cont")
    write_token(ctx, "vv-msix-saved")
    if not wait_phase(ctx, "vv-msix-snapshot-mutated"):
        return
    if ctx.backend.name == "ch":
        ch_command(ctx, "pause")
        ch_command(ctx, "delete")
        ch_command(ctx, "restore", [f"source_url=file://{snap_dir}"])
        write_token(ctx, "vv-msix-restored")
        ch_command(ctx, "resume")
    else:
        qmp(ctx, "stop")
        try:
            write_token(ctx, "vv-msix-restored")
            monitor(ctx, "loadvm vv-msix")
        finally:
            qmp(ctx, "cont")
    ctx.log("restored MSI-X snapshot after mutating the original guest")
