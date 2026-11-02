# SPDX-License-Identifier: Apache-2.0
"""
N0200 sidecar. Hot adds a virtio-net function.
CH requires permission to create a tap interface on the host.
"""
from villain.sidecar_helpers import ch_command, qmp, wait_ready

SUPPORTED_BACKENDS = ("ch", "qemu")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def run(ctx):
    wait_ready(ctx)
    if ctx.backend.name == "ch":
        ch_command(ctx, "add-net", ["tap=,id=hp_net,mac="])
    else:
        qmp(ctx, "netdev_add", {"type": "user", "id": "hpnet"})
        qmp(ctx, "device_add", {
            "driver": "virtio-net-pci-non-transitional",
            "netdev": "hpnet", "id": "hp_net", "romfile": "",
        })
    ctx.log("add-net completed")
