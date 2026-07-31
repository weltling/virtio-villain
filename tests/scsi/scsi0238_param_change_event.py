# SPDX-License-Identifier: Apache-2.0
"""
SCSI0238 sidecar. Resizes the backing disk of the boot LUN so the
device reports a parameter change event for the capacity change.
QEMU only. On other backends or when the API socket is disabled the
sidecar exits cleanly and the guest test skips.
"""


def run(ctx):
    if ctx.backend.name != "qemu":
        ctx.log("backend is not QEMU, skipping host side resize")
        return
    if ctx.api_sock is None:
        ctx.log("api socket disabled, skipping host side resize")
        return

    if not ctx.wait_text("vv-scsi-armed", timeout=20.0):
        ctx.log("guest did not arm the event queue, aborting")
        return

    # Grow the LUN 0 backing image from 16 MiB to 32 MiB.
    r = ctx.vm_api("block_resize", {
        "device": "scsidisk",
        "size": 32 * 1024 * 1024,
    })
    ctx.log(f"block_resize: {r}")
