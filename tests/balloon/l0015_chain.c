/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0015: Balloon inflate chained descriptors.
 *
 * Build a 3-segment chain of PFN arrays via VRING_DESC_F_NEXT.
 * The device must walk the chain.
 *
 * Spec 2.7.5 and 5.5.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_balloon_chain(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    uint64_t base = vv_virt_to_phys(vv_alloc_pages(3));
    for (int i = 0; i < 3; i++)
        pfns[i] = (uint32_t)((base >> VIRTIO_BALLOON_PFN_SHIFT) + i);
    uint64_t p = vv_virt_to_phys(pfns);

    vring_raw_set_desc(vr, 0, p,     4, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, p + 4, 4, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, p + 8, 4, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0015, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_chain,
              "Inflate multi-segment chain",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
