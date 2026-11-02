/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0027: avail_used_event_poison
 *
 * Set used_event (the word after the last avail ring entry) to a
 * pathological value. If the VMM reads used_event to decide whether
 * to send interrupts, a bogus value could suppress all notifications
 * or cause an infinite interrupt storm.
 */
#include "lib/virtio_test.h"

static test_result_t test_avail_used_event_poison(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Valid descriptor chain */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    /*
     * Poison used_event: it lives at avail->ring[queue_size].
     * Set it to 0xFFFF so the VMM thinks it should never notify.
     */
    vr->avail->ring[vr->size] = 0xFFFF;
    __sync_synchronize();

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0027, VIRTIO_PCI_DEVICE_BLK, test_avail_used_event_poison,
              "Poisoned used_event value",
              VIRTIO_SPEC_V1_2, "2.7.6");
