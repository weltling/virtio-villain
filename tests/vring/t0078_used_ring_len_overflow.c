/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0078: used_ring_element_len_exceeds_buffer
 *
 * Submit a read, then verify that the used ring element's `len` field
 * does not exceed the total writable buffer space provided. A device
 * reporting more bytes written than the buffer can hold indicates a
 * bounds error.
 */
#include "lib/virtio_test.h"

static test_result_t test_used_ring_len_overflow(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Provide exactly 512 + 1 writable bytes */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    /* Check used element len */
    __sync_synchronize();
    uint32_t used_len = vr->used->ring[0].len;
    uint32_t max_writable = 512 + 1; /* data + status */

    if (used_len > max_writable)
        TFAIL("used_len > max_writable"); /* device reports more written than buffer */

    return TEST_PASS;
}

REGISTER_TEST(T0078, VIRTIO_PCI_DEVICE_BLK, test_used_ring_len_overflow,
              "Used ring element len must not exceed writable buffer size",
              VIRTIO_SPEC_V1_2, "2.7.8");
