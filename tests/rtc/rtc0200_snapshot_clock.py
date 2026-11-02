# SPDX-License-Identifier: Apache-2.0
"""RTC0200 sidecar. Saves VM state while the guest checks its clock."""

from villain.sidecar_helpers import snapshot, snapshot_config

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = snapshot


def vmm_config():
    return snapshot_config("rtc")
