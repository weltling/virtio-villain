/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0042: packed_no_kick_empty_ring
 *
 * Leave every descriptor slot in its initial state with both
 * AVAIL and USED bits clear, then kick. Spec 2.8.6 says the
 * device must observe no available descriptors and produce no
 * completions. The device must not consume garbage.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_packed_empty(struct virtio_dev *dev,
                                       struct vring_packed *vr)
{
    /* Snapshot AVAIL/USED of slot 0; should remain (0,0) after kick */
    uint16_t before = vr->desc[0].flags;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        uint16_t now = vr->desc[0].flags;
        if (now != before)
            TFAIL("now != before");
        elapsed += 10000;
    }

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0042, VIRTIO_PCI_DEVICE_BLK, test_packed_empty,
                     "Kick on a fully empty packed ring",
                     VIRTIO_SPEC_V1_2, "2.8.6");
