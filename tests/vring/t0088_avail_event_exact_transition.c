/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0088: Set avail_event to exactly the next expected idx then submit.
 *
 * Spec 2.7.7.1: Notification suppression via VIRTQ_AVAIL_EVENT.
 * When avail_event equals the next avail idx the driver publishes,
 * the device MUST be notified. Tests the exact boundary condition.
 */
#include "lib/virtio_test.h"

static test_result_t test_avail_event_exact(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Set used_event (in the avail ring, after the ring entries) to
     * exactly 0, which is where used->idx should be pointing.
     * This means the device should notify us on the very first completion.
     *
     * The avail_event field is at used->ring[queue_size].id position
     * (spec 2.7.7.1). Set it to exactly the next avail idx we will
     * publish (which is 1).
     */
    uint16_t *avail_event = (uint16_t *)&vr->used->ring[vr->size];
    *avail_event = 1; /* notify when avail idx reaches 1 */
    __sync_synchronize();

    /* Enable VIRTQ_AVAIL_F_NO_INTERRUPT is NOT set; use event idx mode */
    vr->avail->flags = 0; /* no suppression from driver side */
    __sync_synchronize();

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0088, VIRTIO_PCI_DEVICE_BLK, test_avail_event_exact,
              "Notification suppression boundary: avail_event == next idx",
              VIRTIO_SPEC_V1_2, "2.7.7.1");
