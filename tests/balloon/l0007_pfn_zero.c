/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0007: Balloon inflate with PFN=0.
 *
 * Submit PFN value 0 (physical address 0) to inflateq. Address 0
 * is typically reserved/unmapped; the device must safely refuse
 * or ignore rather than crash.
 *
 * Spec 5.5.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_pfn_zero(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    pfns[0] = 0;

    vv_desc(vr, 0, pfns, sizeof(uint32_t), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(L0007, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_pfn_zero,
              "Inflate PFN 0",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
