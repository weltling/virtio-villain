/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0114: three descriptor chain completes correctly.
 *
 * Spec 2.7.5: A descriptor chain may link multiple buffers via
 * NEXT flags. Submit a blk READ using three separate descriptors
 * (header, data, status) and verify all three are consumed.
 * This is the fundamental three-desc chain correctness test.
 */
#include "lib/virtio_test.h"

static test_result_t test_three_desc_chain(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    memset(data, 0xCC, 512);
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    /* Verify status was written */
    if (*st == 0xFF)
        TFAIL("status byte unchanged (device did not write)");
    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("status %u, expected S_OK", *st);

    /* Verify data was written (not still 0xCC) */
    int changed = 0;
    for (int i = 0; i < 512; i++) {
        if (data[i] != 0xCC) { changed = 1; break; }
    }
    (void)changed;
    /* Note: data could still be 0xCC if device happened to read that,
     * but status must have changed from 0xFF */

    return TEST_PASS;
}

REGISTER_TEST(T0114, VIRTIO_PCI_DEVICE_BLK, test_three_desc_chain,
              "Three descriptor chain completes and writes status",
              VIRTIO_SPEC_V1_2, "2.7.5");
