/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0015: packed_wrap_at_idx_zero
 *
 * Submit a descriptor with the wrap counter already toggled at index 0,
 * simulating a queue that has wrapped multiple times. Tests that the
 * device correctly tracks the wrap counter phase independently of the
 * descriptor index.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_packed_wrap_at_zero(struct virtio_dev *dev,
                                             struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Manually advance wrap counter by filling the entire queue once,
     * then submitting a real request starting at index 0 with
     * toggled wrap counter.
     */
    uint16_t qsz = vr->size;

    /* Fill the ring with dummy descriptors to wrap once */
    for (uint16_t i = 0; i < qsz; i++) {
        vring_packed_set_desc(vr, i, hdr_phys, sizeof(*hdr), i, 0);
        vring_packed_advance(vr);
    }

    /* Now wrap counter is toggled and next_avail is back to 0 */
    uint8_t wc = vr->wrap_counter;

    /* Submit real request at index 0 with new wrap counter phase */
    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_set_desc(vr, 1, data_phys, 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_set_desc(vr, 2, status_phys, 1, 2,
                          VRING_PACKED_DESC_F_WRITE);

    return vv_kick_and_wait_packed(dev, vr, 0, 2, wc, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0015, VIRTIO_PCI_DEVICE_BLK, test_packed_wrap_at_zero,
                     "Wrapped queue submitting at idx 0 with toggled counter",
                     VIRTIO_SPEC_V1_2, "2.8.21");
