# SPDX-License-Identifier: Apache-2.0
"""RNG0200 sidecar. Saves VM state during an RNG request."""

from villain.sidecar_helpers import snapshot, snapshot_config

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = snapshot


def vmm_config():
    return snapshot_config("rng")
