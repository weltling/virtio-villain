# SPDX-License-Identifier: Apache-2.0
from villain.sidecar_helpers import wait_ready
from villain.msix_host import check_disk, restore_mapping, wait_phase

SUPPORTED_BACKENDS = ("ch", "qemu", "openvmm")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def vmm_config():
    return {"opts": {"vm_snapshot": True, "fuzz_device": "blk"}}


def run(ctx):
    wait_ready(ctx)
    check_disk(ctx)
    if wait_phase(ctx, "vv-msix-snapshot-ready"):
        restore_mapping(ctx)
