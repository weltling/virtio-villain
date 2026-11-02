/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0082: Descriptor with all fields at maximum values (spec 2.7.5)
 *
 * Set addr=UINT64_MAX, len=UINT32_MAX, flags=0xFFFF, next=UINT16_MAX
 * on a descriptor. The device must handle extreme values gracefully.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_max_values(struct virtio_dev *dev,
                                          struct vring *vr)
{
    /* Set descriptor 0 with all maximum values */
    vr->desc[0].addr = 0xFFFFFFFFFFFFFFFFULL;
    vr->desc[0].len = 0xFFFFFFFF;
    vr->desc[0].flags = 0xFFFF;
    vr->desc[0].next = 0xFFFF;

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0082, VIRTIO_PCI_DEVICE_BLK, test_desc_max_values,
              "Descriptor with addr/len/flags/next all at UINT_MAX",
              VIRTIO_SPEC_V1_2, "2.7.5");
