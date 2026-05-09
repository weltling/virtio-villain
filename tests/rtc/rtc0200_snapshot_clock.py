# SPDX-License-Identifier: Apache-2.0
"""
RTC0200 sidecar. Pauses, snapshots, resumes the VM through ch-remote.
"""
import os


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

    snap_dir = os.path.join(ctx.tmpdir, "snap")
    os.makedirs(snap_dir, exist_ok=True)
    cp = ctx.vm_api("pause")
    if cp.returncode != 0:
        ctx.log(f"pause failed rc={cp.returncode} stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote pause failed")
    cp = ctx.vm_api("snapshot", [f"file://{snap_dir}"])
    if cp.returncode != 0:
        ctx.log(f"snapshot failed rc={cp.returncode} stderr={cp.stderr!r}")
        ctx.vm_api("resume")
        raise RuntimeError("ch-remote snapshot failed")
    cp = ctx.vm_api("resume")
    if cp.returncode != 0:
        ctx.log(f"resume failed rc={cp.returncode} stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote resume failed")
    ctx.log("snapshot completed")
