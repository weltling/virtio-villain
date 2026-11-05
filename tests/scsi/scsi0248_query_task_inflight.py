# SPDX-License-Identifier: Apache-2.0
"""
SCSI0248 sidecar. Throttles the boot LUN so a guest read stays queued
while a QUERY TASK asks whether the task is present. QEMU only.
"""

from villain.scsi_helpers import scsi_config, throttle

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
run = throttle
vmm_config = scsi_config
