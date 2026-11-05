# SPDX-License-Identifier: Apache-2.0
"""
SCSI0253 sidecar. Throttles the boot LUN so a guest read stays queued
while a CLEAR ACA is issued. QEMU only.
"""

from villain.scsi_helpers import scsi_config, throttle

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
run = throttle
vmm_config = scsi_config
