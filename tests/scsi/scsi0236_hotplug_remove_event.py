# SPDX-License-Identifier: Apache-2.0
"""
SCSI0236 sidecar. Hot adds a scsi-hd logical unit and then removes it
so the device delivers a transport reset event with the removed
reason. QEMU only.
"""

import time

from villain.scsi_helpers import hot_add, qmp, scsi_config, wait_armed

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
vmm_config = scsi_config


def run(ctx):
    wait_armed(ctx)
    hot_add(ctx)
    time.sleep(1.5)
    qmp(ctx, "device_del", {"id": "hpdev"})
    ctx.log("hot unplug completed")
