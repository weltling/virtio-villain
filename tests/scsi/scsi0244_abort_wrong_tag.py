# SPDX-License-Identifier: Apache-2.0
"""
SCSI0244 sidecar. Throttles the boot LUN so a guest read stays queued
while a mismatched ABORT TASK is issued. QEMU only.
"""

from villain.scsi_helpers import scsi_config, throttle

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
run = throttle
vmm_config = scsi_config
