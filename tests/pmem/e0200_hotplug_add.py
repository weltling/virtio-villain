# SPDX-License-Identifier: Apache-2.0
"""
E0200 sidecar. Hot adds a virtio-pmem region.
Backs it with a fresh sparse file in ctx.tmpdir.
"""
import os

from villain.sidecar_helpers import ch_command, qmp, wait_ready

SUPPORTED_BACKENDS = ("ch", "qemu")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}
HOTPLUG_PMEM_SIZE = 16 * 1024 * 1024


def run(ctx):
    wait_ready(ctx)
    from villain import images

    pmem_file = os.path.join(ctx.tmpdir, "hp_pmem.raw")
    images.create_raw(pmem_file, HOTPLUG_PMEM_SIZE)
    if ctx.backend.name == "ch":
        ch_command(ctx, "add-pmem",
                   [f"file={pmem_file},size={HOTPLUG_PMEM_SIZE},id=hp_pmem"])
    else:
        qmp(ctx, "object-add", {
            "qom-type": "memory-backend-file", "id": "hppmem",
            "mem-path": pmem_file, "size": HOTPLUG_PMEM_SIZE,
        })
        qmp(ctx, "device_add", {
            "driver": "virtio-pmem-pci", "id": "hp_pmem", "memdev": "hppmem",
        })
    ctx.log("add-pmem completed")
