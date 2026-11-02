# SPDX-License-Identifier: Apache-2.0
from villain.sidecar_helpers import pause_resume, wait_ready
from villain.msix_host import check_disk, wait_phase, write_token

SUPPORTED_BACKENDS = ("ch", "qemu", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def run(ctx):
    wait_ready(ctx)
    check_disk(ctx)
    if not wait_phase(ctx, "vv-msix-pause-ready"):
        return
    pause_resume(ctx)
    write_token(ctx, "vv-msix-pause-done")
