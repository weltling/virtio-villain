# SPDX-License-Identifier: Apache-2.0
"""
SCSI0237 sidecar. Hot adds a scsi-hd logical unit while the guest
event queue is empty so the device drops the event and later reports
it with the events missed flag. QEMU only.
"""

from villain.scsi_helpers import hot_add, scsi_config, wait_armed

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
vmm_config = scsi_config


def run(ctx):
    wait_armed(ctx)
    hot_add(ctx)
    ctx.log("hot add completed")
