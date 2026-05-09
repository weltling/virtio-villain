/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0021: balloon_inflate_pfn_aliases_desc_table
 *
 * Inflate a PFN list whose first entry equals the PFN of the
 * vring descriptor table. If the device honored the inflate it
 * would later corrupt its own queue. Spec 5.5.6.1 leaves PFN
 * validity to the device, but it must not wedge or panic.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BALLOON_PFN_SHIFT 12

static test_result_t test_balloon_inflate_alias(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);

    /* Alias the descriptor table page */
    pfns[0] = (uint32_t)(vr->desc_phys >> VIRTIO_BALLOON_PFN_SHIFT);
    pfns[1] = (uint32_t)(vr->avail_phys >> VIRTIO_BALLOON_PFN_SHIFT);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pfns), 2 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0021, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate_alias,
              "Inflate PFN aliasing the vring desc table",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
