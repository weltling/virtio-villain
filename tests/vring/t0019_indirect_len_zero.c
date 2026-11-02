/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0019: indirect_len_zero
 *
 * Set an indirect descriptor's len to 0. This means the indirect table
 * contains zero descriptors - there is no request to process.
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_len_zero(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);

    /*
     * Main descriptor: INDIRECT with len = 0 (empty table).
     */
    vv_desc(vr, 0, page, 0, VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0019, VIRTIO_PCI_DEVICE_BLK, test_indirect_len_zero,
              "Indirect descriptor with len = 0",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
