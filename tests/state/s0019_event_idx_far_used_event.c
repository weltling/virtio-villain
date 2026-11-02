/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0019: event_idx_ignore_used_event
 *
 * Negotiate VIRTIO_F_EVENT_IDX (bit 29) then set the used_event
 * field to a value far in the future (e.g., 0xFFFF). The device
 * should still process requests but may suppress notifications.
 * This tests the device's used_event parsing under edge values.
 */
#include "lib/virtio_test.h"

static test_result_t test_event_idx_far_used_event(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    /*
     * Set used_event to 0xFFFF (far future).
     * The used_event lives at avail->ring[queue_size] per spec 2.7.14.
     */
    uint16_t qsz = vr->size;
    vr->avail->ring[qsz] = 0xFFFF;
    __sync_synchronize();

    /* Also set AVAIL_NO_INTERRUPT in avail flags for extra confusion */
    vr->avail->flags = 1; /* VRING_AVAIL_F_NO_INTERRUPT */
    __sync_synchronize();

    /* Submit a normal read request */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0019, VIRTIO_PCI_DEVICE_BLK, test_event_idx_far_used_event,
              "EVENT_IDX with used_event set to 0xFFFF",
              VIRTIO_SPEC_V1_2, "2.7.14");
