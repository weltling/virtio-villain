# SPDX-License-Identifier: Apache-2.0
"""
SCSI0238 sidecar. Resizes the backing disk of the boot LUN so the
device reports a parameter change event for the capacity change.
QEMU only.
"""

from villain.scsi_helpers import qmp, scsi_config, wait_armed

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
vmm_config = scsi_config


def run(ctx):
    wait_armed(ctx)
    qmp(ctx, "block_resize", {
        "device": "scsidisk",
        "size": 32 * 1024 * 1024,
    })
    ctx.log("block resize completed")
