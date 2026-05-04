/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0005: Balloon inflate with non-4-byte-aligned descriptor length.
 *
 * Submit a descriptor whose length is not a multiple of 4 bytes
 * (PFN size). The device must handle the truncated/malformed entry.
 *
 * Spec 5.5.6.1: Robustness test.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BALLOON_PFN_SHIFT 12

static test_result_t test_balloon_inflate_odd_len(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    uint64_t base = vv_virt_to_phys(vv_alloc_pages(1));
    pfns[0] = (uint32_t)(base >> VIRTIO_BALLOON_PFN_SHIFT);

    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    /* 5 bytes instead of 4 — not aligned to PFN entry size */
    vring_raw_set_desc(vr, 0, pfns_phys, 5, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0005, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate_odd_len,
              "Inflate with non-multiple-of-4 descriptor length",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
