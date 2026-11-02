/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0113: used ring entry id matches avail ring head descriptor.
 *
 * Spec 2.7.8: The device writes the head descriptor index into
 * the id field of each used ring entry. Submit a request starting
 * at descriptor 0, verify used->ring[0].id == 0.
 */
#include "lib/virtio_test.h"

static test_result_t test_used_id(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    uint32_t id = vr->used->ring[0].id;
    if (id != 0)
        TFAIL("used id %u, expected 0 (head descriptor)", id);

    return TEST_PASS;
}

REGISTER_TEST(T0113, VIRTIO_PCI_DEVICE_BLK, test_used_id,
              "Used ring entry id matches head descriptor index",
              VIRTIO_SPEC_V1_2, "2.7.8");
