# SPDX-License-Identifier: Apache-2.0
"""B0201 sidecar. Removes and readds a disk with guest handshakes."""

from villain.sidecar_helpers import add_disk, ch_command, readd_disk, remove_disk, wait_ready
import os

SUPPORTED_BACKENDS = ("ch", "qemu")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def run(ctx):
    wait_ready(ctx)
    add_disk(ctx)
    if not ctx.wait_text("vv-blk-added", timeout=20.0):
        raise RuntimeError("guest did not discover the first hot added disk")
    remove_disk(ctx)
    if not ctx.wait_text("vv-blk-removed", timeout=20.0):
        raise RuntimeError("guest did not observe hot disk removal")
    if ctx.backend.name == "ch":
        hp_disk = os.path.join(ctx.tmpdir, "hp.raw")
        ch_command(ctx, "add-disk",
                   [f"path={hp_disk},readonly=off,id=hp0,image_type=raw"])
    else:
        readd_disk(ctx)
    ctx.log("second add-disk completed")
