# SPDX-License-Identifier: Apache-2.0
"""
V0200 sidecar. Hot adds a vsock function with cid 44 and id hp_vsock.
"""
import os

from villain.sidecar_helpers import qmp, wait_ready

SUPPORTED_BACKENDS = ("ch", "qemu")
REQUIRED_TOOLS = {"ch": ("ch-remote",)}


def vmm_config():
    return {"opts": {"vsock_enabled": False}}


def run(ctx):
    wait_ready(ctx)

    if ctx.backend.name == "qemu":
        qmp(ctx, "device_add", {
            "driver": "vhost-vsock-pci", "id": "hp_vsock",
            "guest-cid": ctx.vmm_pid + 3,
        })
        ctx.log("add-vsock completed")
        return

    socket = os.path.join(ctx.tmpdir, "vsock.sock")
    cp = ctx.vm_api("add-vsock", [f"cid=44,id=hp_vsock,socket={socket}"])
    if cp.returncode != 0:
        raise RuntimeError(
            f"add-vsock failed rc={cp.returncode} "
            f"stdout={cp.stdout!r} stderr={cp.stderr!r}")
    ctx.log("add-vsock completed")
