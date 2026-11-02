/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0055: notif_suppression_overflow
 *
 * Set avail_event (used_event in the available ring) to a value that
 * would cause a wrapping overflow when compared to avail.idx.
 * A VMM using naive signed comparison may miscalculate whether to
 * send a notification.
 */
#include "lib/virtio_test.h"

static test_result_t test_notif_suppression_overflow(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    /*
     * Set used_event (at avail->ring[qsz]) to 0xFFFE.
     * Then set avail.idx to 1. The wrapping difference is huge,
     * which may confuse notification suppression logic.
     */
    uint16_t qsz = vr->size;
    volatile uint16_t *used_event =
        (volatile uint16_t *)((char *)vr->avail + 4 + qsz * 2);
    *used_event = 0xFFFE;
    __sync_synchronize();

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0055, VIRTIO_PCI_DEVICE_BLK, test_notif_suppression_overflow,
              "Notification suppression with wrapping event idx",
              VIRTIO_SPEC_V1_2, "2.7.13");
