# SPDX-License-Identifier: Apache-2.0
"""A0200 sidecar. Saves VM state with an active admin queue."""

from villain.sidecar_helpers import snapshot, snapshot_config

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = snapshot
vmm_config = snapshot_config
