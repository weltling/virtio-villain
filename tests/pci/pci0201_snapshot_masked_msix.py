# SPDX-License-Identifier: Apache-2.0
"""PCI0201 sidecar. Saves VM state while MSI-X vectors are masked."""

from villain.sidecar_helpers import snapshot, snapshot_config

SUPPORTED_BACKENDS = ("ch", "qemu", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = snapshot
vmm_config = snapshot_config
