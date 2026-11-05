# SPDX-License-Identifier: Apache-2.0
"""Shared QEMU SCSI host actions."""

import os

from villain.sidecar_helpers import HOTPLUG_DISK_SIZE, qmp


def wait_armed(ctx):
    if ctx.backend.name not in ("qemu", "qemu-mmio"):
        ctx.skip("backend is not QEMU")
    if ctx.api_sock is None:
        ctx.skip("api socket disabled")
    if not ctx.wait_text("vv-scsi-armed", timeout=20.0):
        raise RuntimeError("guest did not arm the event queue")


def scsi_config():
    return {"opts": {"mmio_device": "scsi"}}


def hot_add(ctx):
    from villain import images

    hp = os.path.join(ctx.tmpdir, "hp.raw")
    images.create_raw(hp, HOTPLUG_DISK_SIZE)
    ctx.log(f"created hot plug image {hp}")
    qmp(ctx, "blockdev-add", {
        "driver": "raw",
        "node-name": "hpnode",
        "file": {"driver": "file", "filename": hp},
    })
    qmp(ctx, "device_add", {
        "driver": "scsi-hd",
        "drive": "hpnode",
        "bus": "scsi0.0",
        "channel": 0,
        "scsi-id": 0,
        "lun": 2,
        "id": "hpdev",
    })


def throttle(ctx):
    wait_armed(ctx)
    qmp(ctx, "block_set_io_throttle", {
        "device": "scsidisk",
        "bps": 0, "bps_rd": 0, "bps_wr": 0,
        "iops": 1, "iops_rd": 0, "iops_wr": 0,
    })
    ctx.log("I/O throttle configured")
