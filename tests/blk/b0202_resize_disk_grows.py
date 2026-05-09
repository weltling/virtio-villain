# SPDX-License-Identifier: Apache-2.0
"""
B0202 sidecar. Grows the boot disk on the host through resize-disk.
The guest watches device_cfg.capacity and config_generation.
"""
import os
import subprocess


def run(ctx):
    if ctx.backend.name != "ch":
        ctx.log("backend is not CH, skipping")
        return
    if ctx.api_sock is None:
        ctx.log("api socket disabled, skipping")
        return
    if not ctx.wait_text("[vv]", timeout=20.0):
        ctx.log("guest harness banner not seen")
        return

    cur = os.path.getsize(ctx.disk)
    new = cur + 16 * 1024 * 1024
    ctx.log(f"resizing disk from {cur} to {new} bytes")
    try:
        cp = ctx.vm_api("resize-disk",
                        ["--disk", "_disk0", "--size", str(new)],
                        timeout=30.0)
    except subprocess.TimeoutExpired:
        ctx.log("resize-disk timed out, vmm probably already exited")
        return
    if cp.returncode != 0:
        ctx.log(f"resize-disk failed rc={cp.returncode} "
                f"stdout={cp.stdout!r} stderr={cp.stderr!r}")
        return
    ctx.log("resize-disk completed")
