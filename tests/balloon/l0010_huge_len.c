/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0010: Balloon inflate with huge length.
 *
 * Submit a descriptor whose len claims 1 GiB of PFNs (256M
 * entries) backed by a single page. Device must clamp or refuse.
 *
 * Spec 5.5.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_huge_len(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    vring_raw_set_desc(vr, 0, pfns_phys, 1u << 30, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0010, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_huge_len,
              "Inflate with 1 GiB length",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
