# SPDX-License-Identifier: Apache-2.0
"""Shared checked host actions for test sidecars."""

import os
import time

HOTPLUG_DISK_SIZE = 16 * 1024 * 1024


def wait_ready(ctx):
    if ctx.backend.name not in ("ch", "qemu", "qemu-mmio", "openvmm"):
        ctx.skip("backend is not CH, QEMU, QEMU MMIO or OpenVMM")
    if ctx.api_sock is None:
        ctx.skip("api socket disabled")
    if not ctx.wait_text("[vv]", timeout=20.0):
        raise RuntimeError("guest harness banner not seen")
    if ctx.backend.name == "qemu-mmio":
        if not ctx.wait_text("vv-mmio-ready", timeout=20.0):
            raise RuntimeError("guest MMIO device not ready")


def qmp(ctx, command, arguments=None):
    response = ctx.vm_api(command, arguments)
    ctx.log(f"{command}: {response}")
    if (not isinstance(response, dict) or "error" in response
            or "return" not in response):
        raise RuntimeError(f"QMP {command} failed: {response!r}")
    return response


def ch_command(ctx, command, arguments=None):
    response = ctx.vm_api(command, arguments)
    if response.returncode != 0:
        ctx.log(f"{command} failed rc={response.returncode} "
                f"stdout={response.stdout!r} stderr={response.stderr!r}")
        client = "OpenVMM" if ctx.backend.name == "openvmm" else "ch-remote"
        raise RuntimeError(f"{client} {command} failed")
    return response


def pause_resume(ctx):
    wait_ready(ctx)
    if ctx.backend.name in ("ch", "openvmm"):
        ch_command(ctx, "pause")
    else:
        qmp(ctx, "stop")
    try:
        time.sleep(0.5)
    finally:
        if ctx.backend.name in ("ch", "openvmm"):
            ch_command(ctx, "resume")
        else:
            qmp(ctx, "cont")
    ctx.log("paused and resumed")


def add_disk(ctx):
    from villain import images

    hp_disk = os.path.join(ctx.tmpdir, "hp.raw")
    images.create_raw(hp_disk, HOTPLUG_DISK_SIZE)
    if ctx.backend.name == "ch":
        ch_command(ctx, "add-disk",
                   [f"path={hp_disk},readonly=off,id=hp0,image_type=raw"])
    else:
        qmp(ctx, "blockdev-add", {
            "driver": "raw", "node-name": "hpnode",
            "file": {"driver": "file", "filename": hp_disk},
        })
        readd_disk(ctx)
    ctx.log("add-disk completed")


def readd_disk(ctx):
    qmp(ctx, "device_add", {
        "driver": "virtio-blk-pci-non-transitional",
        "drive": "hpnode", "id": "hp0",
    })


def remove_disk(ctx):
    if ctx.backend.name == "ch":
        ch_command(ctx, "remove-device", ["hp0"])
    else:
        qmp(ctx, "device_del", {"id": "hp0"})
        deadline = time.monotonic() + 5
        while True:
            reply = qmp(ctx, "qom-list", {"path": "/machine/peripheral"})
            devices = reply["return"]
            if not isinstance(devices, list) or any(
                    not isinstance(device, dict)
                    or not isinstance(device.get("name"), str)
                    for device in devices):
                raise RuntimeError(f"QMP qom-list failed: {reply!r}")
            if not any(device["name"] == "hp0" for device in devices):
                break
            if time.monotonic() >= deadline:
                raise TimeoutError("QEMU did not complete device_del for hp0")
            time.sleep(0.02)
    ctx.log("remove-device completed")


def snapshot_config(mmio_device="blk"):
    return {"opts": {"vm_snapshot": True, "mmio_device": mmio_device}}


def snapshot(ctx):
    wait_ready(ctx)
    if ctx.backend.name == "openvmm":
        ch_command(ctx, "pulse-save-restore")
        ctx.log("VM device state saved and restored")
        return
    snap_dir = os.path.join(ctx.tmpdir, "snap")
    os.makedirs(snap_dir, exist_ok=True)
    if ctx.backend.name == "ch":
        ch_command(ctx, "pause")
        try:
            ch_command(ctx, "snapshot", [f"file://{snap_dir}"])
        finally:
            ch_command(ctx, "resume")
    else:
        qmp(ctx, "stop")
        try:
            qmp(ctx, "migrate", {"uri": f"file:{snap_dir}/vmstate"})
            deadline = time.monotonic() + 20
            while True:
                reply = qmp(ctx, "query-migrate")
                migration = reply["return"]
                if not isinstance(migration, dict):
                    raise RuntimeError(f"QMP query-migrate failed: {reply!r}")
                state = migration.get("status")
                if state == "completed":
                    break
                if state in ("failed", "cancelled") or state is None:
                    raise RuntimeError(f"QMP query-migrate failed: {reply!r}")
                if time.monotonic() >= deadline:
                    raise TimeoutError("QMP query-migrate timed out")
                time.sleep(0.02)
            state_file = os.path.join(snap_dir, "vmstate")
            size = os.path.getsize(state_file)
            if size == 0:
                raise RuntimeError("QEMU saved an empty VM state file")
            ctx.log(f"saved VM state: {size} bytes")
        finally:
            qmp(ctx, "cont")
    ctx.log("snapshot completed")
