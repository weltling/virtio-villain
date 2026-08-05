# SPDX-License-Identifier: Apache-2.0
"""
SCSI0254 sidecar. Throttles the boot LUN so a guest eight block read
stays queued long enough to be aborted by tag. QEMU only.
"""


def run(ctx):
    if ctx.backend.name != "qemu":
        ctx.log("backend is not QEMU, skipping host side throttle")
        return
    if ctx.api_sock is None:
        ctx.log("api socket disabled, skipping host side throttle")
        return
    if not ctx.wait_text("vv-scsi-armed", timeout=20.0):
        ctx.log("guest did not arm, aborting")
        return
    r = ctx.vm_api("block_set_io_throttle", {
        "device": "scsidisk",
        "bps": 0, "bps_rd": 0, "bps_wr": 0,
        "iops": 1, "iops_rd": 0, "iops_wr": 0,
    })
    ctx.log(f"block_set_io_throttle: {r}")
