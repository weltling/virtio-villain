/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0033: indirect_max_entries
 *
 * With VIRTIO_F_INDIRECT_DESC negotiated a request uses a full
 * indirect table of writable entries. The device must fill the whole
 * table without overrunning it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define ENTRIES 16

static test_result_t test_rng_indirect_max_entries(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct vring_desc *itab = vv_alloc_pages(1);
    uint8_t *buf = vv_alloc_pages(1);

    for (int i = 0; i < ENTRIES; i++) {
        uint16_t last = i == ENTRIES - 1;
        itab[i].addr = vv_virt_to_phys(buf + (i * 128 % 4096));
        itab[i].len = 128;
        itab[i].flags = VRING_DESC_F_WRITE | (last ? 0 : VRING_DESC_F_NEXT);
        itab[i].next = last ? 0 : i + 1;
    }

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(itab),
                       ENTRIES * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(RNG0033, VIRTIO_PCI_DEVICE_RNG,
                       test_rng_indirect_max_entries,
                       "RNG indirect table using a full entry set",
                       VIRTIO_SPEC_V1_2, "2.7.7",
                       (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
