/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0014: Balloon inflate fill ring.
 *
 * Place 16 distinct one-PFN descriptors into the inflateq, one
 * per slot, and advance avail->idx by 16. The device must drain
 * them all without losing entries.
 *
 * Spec 5.5.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_balloon_fill_ring(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    uint8_t *base = vv_alloc_pages(vr->size);
    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    for (uint16_t i = 0; i < vr->size; i++) {
        uint64_t pa = vv_virt_to_phys(base + (size_t)i * PAGE_SIZE);
        pfns[i] = (uint32_t)(pa >> VIRTIO_BALLOON_PFN_SHIFT);
        vring_raw_set_desc(vr, i, pfns_phys + i * 4, 4, 0, 0);
        vring_raw_set_avail(vr, i, i);
    }
    vring_raw_set_avail_idx(vr, vr->size);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0014, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_fill_ring,
              "Inflate fill the ring",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
