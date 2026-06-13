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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_pfn_zero(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    pfns[0] = 0;
    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    vring_raw_set_desc(vr, 0, pfns_phys, sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0007, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_pfn_zero,
              "Inflate PFN 0",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
