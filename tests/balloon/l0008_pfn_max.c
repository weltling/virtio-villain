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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_pfn_max(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    pfns[0] = 0xFFFFFFFFu;
    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    vring_raw_set_desc(vr, 0, pfns_phys, sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0008, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_pfn_max,
              "Inflate maximum PFN",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
