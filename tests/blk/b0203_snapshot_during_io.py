# SPDX-License-Identifier: Apache-2.0
"""B0203 sidecar. Saves VM state while block I/O is active."""

from villain.sidecar_helpers import snapshot, snapshot_config

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = snapshot
vmm_config = snapshot_config
