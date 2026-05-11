/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0044: packed_avail_without_used_clear
 *
 * Set the AVAIL flag on a descriptor but also set the USED flag.
 * Spec 2.8.1 says a descriptor is available when AVAIL != USED.
 * When AVAIL == USED the descriptor is consumed; presenting both
 * set from the driver side is ambiguous. The device must not
 * enter an infinite loop or crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_avail_used(struct virtio_dev *dev,
                                            struct vring_packed *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 512);

    vr->desc[0].addr = vv_virt_to_phys(buf);
    vr->desc[0].len = 512;
    vr->desc[0].id = 0;
    /* Both AVAIL and USED set: ambiguous ownership */
    vr->desc[0].flags = VRING_PACKED_DESC_F_AVAIL |
                         VRING_PACKED_DESC_F_USED;
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0044, VIRTIO_PCI_DEVICE_BLK, test_packed_avail_used,
                     "Packed descriptor with both AVAIL and USED set",
                     VIRTIO_SPEC_V1_2, "2.8.1");
