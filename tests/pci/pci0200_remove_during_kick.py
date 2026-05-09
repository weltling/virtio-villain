# SPDX-License-Identifier: Apache-2.0
"""
PCI0200 sidecar. Hot adds a sibling blk function, waits, then
removes it. The guest watches the boot blk function's device_status.
"""
import os
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

    hp_disk = os.path.join(ctx.tmpdir, "pci0200.raw")
    with open(hp_disk, "wb") as f:
        f.truncate(16 * 1024 * 1024)

    cp = ctx.vm_api("add-disk",
                    [f"path={hp_disk},readonly=off,id=hp0,image_type=raw"])
    if cp.returncode != 0:
        ctx.log(f"add-disk failed rc={cp.returncode} stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote add-disk failed")
    ctx.log("add-disk completed")
    time.sleep(1.0)
    cp = ctx.vm_api("remove-device", ["hp0"])
    if cp.returncode != 0:
        ctx.log(f"remove-device failed rc={cp.returncode} "
                f"stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote remove-device failed")
    ctx.log("remove-device completed")
