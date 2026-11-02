# SPDX-License-Identifier: Apache-2.0
"""PCI0200 sidecar. Removes a hot added sibling block device."""

import time

from villain.sidecar_helpers import add_disk, remove_disk, wait_ready

SUPPORTED_BACKENDS = ("ch", "qemu")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def run(ctx):
    wait_ready(ctx)
    add_disk(ctx)
    time.sleep(1.0)
    remove_disk(ctx)
