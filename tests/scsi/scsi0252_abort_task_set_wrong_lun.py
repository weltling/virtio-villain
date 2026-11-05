# SPDX-License-Identifier: Apache-2.0
"""
SCSI0252 sidecar. Throttles the boot LUN so a guest read on LUN 0
stays queued while an ABORT TASK SET targets a different LUN.
QEMU only.
"""

from villain.scsi_helpers import scsi_config, throttle

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
run = throttle
vmm_config = scsi_config
