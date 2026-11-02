/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0072: indirect_zero_valid_entries
 *
 * Set up an indirect descriptor table with len indicating entries
 * but the first entry has addr=0 and len=0. The indirect table
 * technically exists but contains no useful data.
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_zero_valid(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /*
     * Allocate an indirect table of 3 entries (48 bytes) but fill
     * all entries with zeroed addr/len. The device should either
     * reject (addr=0 is invalid) or not crash parsing them.
     */
    struct vring_desc *indirect = vv_alloc_pages(1);

    /* Point main descriptor at the indirect table with INDIRECT flag */
    vv_desc(vr, 0, indirect, 48, VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0072, VIRTIO_PCI_DEVICE_BLK, test_indirect_zero_valid,
              "Indirect table with all-zero descriptor entries",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
