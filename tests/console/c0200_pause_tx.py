# SPDX-License-Identifier: Apache-2.0
"""
C0200 sidecar. Pauses and resumes the VM around a console TX flush.
"""
from villain.sidecar_helpers import pause_resume

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = pause_resume


def vmm_config():
    return {"opts": {"mmio_device": "console"}}
