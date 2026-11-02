# SPDX-License-Identifier: Apache-2.0
"""
B0200 sidecar. Hot adds a virtio-blk function on the host side
while the guest waits for a new PCI device.
"""
from villain.sidecar_helpers import add_disk, wait_ready

SUPPORTED_BACKENDS = ("ch", "qemu")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def run(ctx):
    wait_ready(ctx)
    add_disk(ctx)
