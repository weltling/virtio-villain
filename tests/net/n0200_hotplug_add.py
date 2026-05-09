# SPDX-License-Identifier: Apache-2.0
"""
N0200 sidecar. Hot adds a virtio-net function through ch-remote.
Requires a usable tap interface on the host. If the backend cannot
satisfy the request the guest test surfaces SKIP.
"""
import shutil


def run(ctx):
    if ctx.backend.name != "ch":
        ctx.log("backend is not CH, skipping")
        return
    if ctx.api_sock is None:
        ctx.log("api socket disabled, skipping")
        return
    if not ctx.wait_text("[vv]", timeout=20.0):
        ctx.log("guest harness banner not seen")
        return

    ip = shutil.which("ip")
    if ip is None:
        ctx.log("ip(8) not available, skipping host net setup")
        return

    cp = ctx.vm_api("add-net", ["tap=,id=hp_net,mac="])
    if cp.returncode != 0:
        ctx.log(f"add-net failed rc={cp.returncode} "
                f"stdout={cp.stdout!r} stderr={cp.stderr!r}")
        return
    ctx.log("add-net completed")
