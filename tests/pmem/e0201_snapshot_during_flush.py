# SPDX-License-Identifier: Apache-2.0
"""E0201 sidecar. Saves VM state during a pmem flush."""

from villain.sidecar_helpers import snapshot, snapshot_config

SUPPORTED_BACKENDS = ("ch", "qemu", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = snapshot
vmm_config = snapshot_config
