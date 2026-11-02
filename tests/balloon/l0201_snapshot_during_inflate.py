# SPDX-License-Identifier: Apache-2.0
"""L0201 sidecar. Saves VM state during balloon inflation."""

from villain.sidecar_helpers import snapshot, snapshot_config

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = snapshot


def vmm_config():
    return snapshot_config("balloon")
