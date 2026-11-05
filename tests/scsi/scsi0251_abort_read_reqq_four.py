# SPDX-License-Identifier: Apache-2.0
"""
SCSI0251 sidecar. Throttles the boot LUN so a guest read on the third
request queue stays queued long enough to be aborted. QEMU only.
"""

from villain.scsi_helpers import scsi_config, throttle

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
run = throttle
vmm_config = scsi_config
