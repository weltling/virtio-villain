# SPDX-License-Identifier: Apache-2.0
"""
SCSI0250 sidecar. Throttles the boot LUN so a guest read on the second
request queue stays queued long enough to be aborted. QEMU only.
"""

from villain.scsi_helpers import scsi_config, throttle

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
run = throttle
vmm_config = scsi_config
