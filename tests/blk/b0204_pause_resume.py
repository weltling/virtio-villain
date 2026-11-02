# SPDX-License-Identifier: Apache-2.0
"""
B0204 sidecar. Pauses then resumes the running VM. The guest does
a read on either side of the pause window and asserts both succeed.
"""
from villain.sidecar_helpers import pause_resume

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
run = pause_resume
