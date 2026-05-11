/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0043: packed_desc_id_equals_queue_size
 *
 * Set the buffer ID field of a packed descriptor to queue_size
 * (one past the maximum valid index). Spec 2.8.6 says buffer IDs
 * must be in 0..queue_size-1. An out of bounds ID must not cause
 * an array overflow in the device model.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_id_oob(struct virtio_dev *dev,
                                        struct vring_packed *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 512);

    uint16_t qsz = dev->common->queue_size;

    vr->desc[0].addr = vv_virt_to_phys(buf);
    vr->desc[0].len = 512;
    vr->desc[0].id = qsz;  /* OOB: valid range is 0..qsz-1 */
    vr->desc[0].flags = VRING_PACKED_DESC_F_AVAIL;
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0043, VIRTIO_PCI_DEVICE_BLK, test_packed_id_oob,
                     "Packed descriptor ID equals queue_size",
                     VIRTIO_SPEC_V1_2, "2.8.6");
