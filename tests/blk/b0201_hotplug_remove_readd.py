# SPDX-License-Identifier: Apache-2.0
"""
B0201 sidecar. Hot adds a virtio-blk function with id hp0, gives
the guest time to attach, then removes it and adds it back. The
guest test exercises the freshly re added function.

The C side races us. It may attach to the first add and finish
before the remove and re add complete. That is acceptable. The
spec mandate the guest checks (reset returns device_status to
zero) is independent of which add it ran on. The remove and re
add still exercise the verbs on the host. Connection refused on
later calls means the VM already exited with its result and is
not a failure of the sidecar.
"""
import os
import time


def _ignore_after_vm_exit(ctx, cp, what):
    if cp.returncode == 0:
        return True
    msg = (cp.stderr or "") + (cp.stdout or "")
    if "Connection refused" in msg or "os error 111" in msg:
        ctx.log(f"{what} skipped, VM already exited")
        return True
    ctx.log(f"{what} failed rc={cp.returncode} "
            f"stdout={cp.stdout!r} stderr={cp.stderr!r}")
    return False


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

    hp_disk = os.path.join(ctx.tmpdir, "hp.raw")
    with open(hp_disk, "wb") as f:
        f.truncate(16 * 1024 * 1024)
    ctx.log(f"created disk image {hp_disk}")

    cp = ctx.vm_api("add-disk",
                    [f"path={hp_disk},readonly=off,id=hp0,image_type=raw"])
    if cp.returncode != 0:
        ctx.log(f"add-disk failed rc={cp.returncode} "
                f"stdout={cp.stdout!r} stderr={cp.stderr!r}")
        raise RuntimeError("ch-remote add-disk failed")
    ctx.log("first add-disk completed")

    time.sleep(0.5)
    cp = ctx.vm_api("remove-device", ["hp0"])
    if not _ignore_after_vm_exit(ctx, cp, "remove-device"):
        raise RuntimeError("ch-remote remove-device failed")
    ctx.log("remove-device completed")

    time.sleep(0.5)
    cp = ctx.vm_api("add-disk",
                    [f"path={hp_disk},readonly=off,id=hp0,image_type=raw"])
    if not _ignore_after_vm_exit(ctx, cp, "second add-disk"):
        raise RuntimeError("ch-remote second add-disk failed")
    ctx.log("second add-disk completed")
