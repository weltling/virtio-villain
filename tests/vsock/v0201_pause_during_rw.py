# SPDX-License-Identifier: Apache-2.0
"""
V0201 sidecar. Pauses then resumes the running VM.
"""
import time


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

    cp = ctx.vm_api("pause")
    if cp.returncode != 0:
        ctx.log(f"pause failed rc={cp.returncode} stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote pause failed")
    time.sleep(0.5)
    cp = ctx.vm_api("resume")
    if cp.returncode != 0:
        ctx.log(f"resume failed rc={cp.returncode} stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote resume failed")
    ctx.log("paused and resumed")
