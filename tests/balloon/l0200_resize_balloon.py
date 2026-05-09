# SPDX-License-Identifier: Apache-2.0
"""
L0200 sidecar. Resizes the balloon target on the host. The guest
watches num_pages and config_generation for the change.
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

    cp = ctx.vm_api("resize", ["--balloon", "64M"])
    if cp.returncode != 0:
        ctx.log(f"resize --balloon failed rc={cp.returncode} "
                f"stdout={cp.stdout!r} stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote resize --balloon failed")
    ctx.log("balloon resized to 64M")
