/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0001: Balloon inflate with valid PFNs.
 *
 * Submit a list of page frame numbers to the inflate queue (queue 0)
 * to tell the device the guest is giving up memory.
 *
 * Spec 5.5.6.1: The driver constructs an array of 4-byte PFN values
 * and sends them on the inflate queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_inflate(struct virtio_dev *dev,
                                          struct vring *vr)
{
    /* Allocate a page to hold PFN array */
    uint32_t *pfns = vv_alloc_pages(1);

    /* Offer 4 pages starting at a known safe address */
    uint64_t base_addr = vv_virt_to_phys(vv_alloc_pages(4));
    for (int i = 0; i < 4; i++)
        pfns[i] = (uint32_t)((base_addr >> VIRTIO_BALLOON_PFN_SHIFT) + i);

    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    vring_raw_set_desc(vr, 0, pfns_phys, 4 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0001, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate,
              "Inflate with valid page frame numbers",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
