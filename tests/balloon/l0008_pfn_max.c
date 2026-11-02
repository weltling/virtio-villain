/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0008: Balloon inflate with maximum PFN.
 *
 * Submit PFN=0xFFFFFFFF (physical address 0xFFFFFFFFFFF000). That
 * address is well outside any plausible guest memory map. The
 * device must refuse or silently drop it.
 *
 * Spec 5.5.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_pfn_max(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    pfns[0] = 0xFFFFFFFFu;

    vv_desc(vr, 0, pfns, sizeof(uint32_t), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(L0008, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_pfn_max,
              "Inflate maximum PFN",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
