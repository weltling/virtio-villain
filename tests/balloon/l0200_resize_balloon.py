# SPDX-License-Identifier: Apache-2.0
"""
L0200 sidecar. Resizes the balloon target on the host. The guest
watches num_pages and config_generation for the change.
"""

from villain.sidecar_helpers import ch_command, qmp, wait_ready

SUPPORTED_BACKENDS = ("ch", "qemu", "qemu-mmio")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def vmm_config():
    return {"opts": {"mmio_device": "balloon"}}


def run(ctx):
    wait_ready(ctx)
    if ctx.backend.name == "ch":
        ch_command(ctx, "resize", ["--balloon", "64M"])
    else:
        reply = qmp(ctx, "query-balloon")
        actual = reply["return"].get("actual")
        if not isinstance(actual, int) or actual <= 64 * 1024 * 1024:
            raise RuntimeError(f"QMP query-balloon invalid memory size: {reply!r}")
        qmp(ctx, "balloon", {"value": actual - 64 * 1024 * 1024})
    ctx.log("balloon resized to 64M")
