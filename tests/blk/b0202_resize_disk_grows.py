# SPDX-License-Identifier: Apache-2.0
"""
B0202 sidecar. Grows the boot disk on the host through resize-disk.
The guest watches device_cfg.capacity and config_generation.
"""
import os

from villain.sidecar_helpers import qmp, wait_ready

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def run(ctx):
    wait_ready(ctx)

    cur = os.path.getsize(ctx.disk)
    new = cur + 16 * 1024 * 1024
    ctx.log(f"resizing disk from {cur} to {new} bytes")
    if ctx.backend.name in ("qemu", "qemu-mmio"):
        qmp(ctx, "block_resize", {"device": "vd0", "size": new})
        ctx.log("block_resize completed")
        return
    cp = ctx.vm_api("resize-disk",
                    ["--disk", "_disk0", "--size", str(new)],
                    timeout=30.0)
    if cp.returncode != 0:
        ctx.log(f"resize-disk failed rc={cp.returncode} "
                f"stdout={cp.stdout!r} stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote resize-disk failed")
    ctx.log("resize-disk completed")
