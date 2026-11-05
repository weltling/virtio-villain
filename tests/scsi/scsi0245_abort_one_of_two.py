# SPDX-License-Identifier: Apache-2.0
"""
SCSI0245 sidecar. Throttles the boot LUN so two guest reads stay
queued while one of them is aborted by tag. QEMU only.
"""

from villain.scsi_helpers import scsi_config, throttle

SUPPORTED_BACKENDS = ("qemu", "qemu-mmio")
run = throttle
vmm_config = scsi_config
