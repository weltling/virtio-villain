# SPDX-License-Identifier: Apache-2.0
"""
V0200 sidecar. Hot adds a vsock function with cid 44 and id hp_vsock.
"""


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

    cp = ctx.vm_api("add-vsock", ["cid=44,id=hp_vsock"])
    if cp.returncode != 0:
        ctx.log(f"add-vsock failed rc={cp.returncode} "
                f"stdout={cp.stdout!r} stderr={cp.stderr!r}")
        return
    ctx.log("add-vsock completed")
