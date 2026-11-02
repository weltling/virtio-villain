# SPDX-License-Identifier: Apache-2.0
from villain.sidecar_helpers import ch_command, qmp, wait_ready
from villain.msix_host import disk_capacity, wait_phase

SUPPORTED_BACKENDS = ("ch", "qemu")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def run(ctx):
    wait_ready(ctx)
    initial = disk_capacity(ctx)
    for phase in range(3):
        if not wait_phase(ctx, f"vv-msix-config-ready-{phase}"):
            return
        size = initial + (phase + 1) * 16 * 1024 * 1024
        if ctx.backend.name == "ch":
            ch_command(ctx, "resize-disk",
                       ["--disk", "_disk0", "--size", str(size)])
        else:
            qmp(ctx, "block_resize", {"device": "vd0", "size": size})
    ctx.log("configuration vector events completed")
