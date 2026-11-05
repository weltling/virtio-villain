# SPDX-License-Identifier: Apache-2.0
"""
SCSI0243 sidecar. Throttles the boot LUN so a guest write stays queued
long enough to be aborted by tag. QEMU only.
"""

from villain.scsi_helpers import scsi_config, throttle

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
run = throttle
vmm_config = scsi_config
