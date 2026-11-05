# SPDX-License-Identifier: Apache-2.0
"""
SCSI0235 sidecar. Hot adds a scsi-hd logical unit on the running
virtio-scsi controller so the device delivers a transport reset
event to the guest event queue. QEMU only.
"""

from villain.scsi_helpers import hot_add, scsi_config, wait_armed

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
vmm_config = scsi_config


def run(ctx):
    wait_armed(ctx)
    hot_add(ctx)
    ctx.log("hot add completed")
