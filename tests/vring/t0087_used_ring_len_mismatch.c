/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0087: Submit a 512-byte read, then check if used ring reports len > 512.
 *
 * Spec 2.7.4.2: The device MUST NOT write more than the total
 * descriptor length. Verify that used_elem.len does not exceed
 * what was offered in the writable descriptors.
 */
#include "lib/virtio_test.h"

static test_result_t test_used_ring_len_mismatch(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    /* Check used ring entry: writable bytes = 512 (data) + 1 (status) = 513 */
    __sync_synchronize();
    uint32_t used_len = vr->used->ring[0].len;

    /* Device must not report more bytes written than offered (513 max) */
    if (used_len > 513)
        TFAIL("used_len > 513");

    return TEST_PASS;
}

REGISTER_TEST(T0087, VIRTIO_PCI_DEVICE_BLK, test_used_ring_len_mismatch,
              "Used ring len must not exceed offered writable bytes",
              VIRTIO_SPEC_V1_2, "2.7.4.2");
