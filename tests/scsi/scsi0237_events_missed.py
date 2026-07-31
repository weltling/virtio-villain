# SPDX-License-Identifier: Apache-2.0
"""
SCSI0237 sidecar. Hot adds a scsi-hd logical unit while the guest
event queue is empty so the device drops the event and later reports
it with the events missed flag. QEMU only. On other backends or when
the API socket is disabled the sidecar exits cleanly and the guest
test skips.
"""
import os


def run(ctx):
    if ctx.backend.name != "qemu":
        ctx.log("backend is not QEMU, skipping host side hot add")
        return
    if ctx.api_sock is None:
        ctx.log("api socket disabled, skipping host side hot add")
        return

    if not ctx.wait_text("vv-scsi-armed", timeout=20.0):
        ctx.log("guest did not arm the event queue, aborting")
        return

    hp = os.path.join(ctx.tmpdir, "hp.raw")
    with open(hp, "wb") as f:
        f.truncate(16 * 1024 * 1024)

    r1 = ctx.vm_api("blockdev-add", {
        "driver": "raw",
        "node-name": "hpnode",
        "file": {"driver": "file", "filename": hp},
    })
    ctx.log(f"blockdev-add: {r1}")

    r2 = ctx.vm_api("device_add", {
        "driver": "scsi-hd",
        "drive": "hpnode",
        "bus": "scsi0.0",
        "channel": 0,
        "scsi-id": 0,
        "lun": 2,
        "id": "hpdev",
    })
    ctx.log(f"device_add: {r2}")
