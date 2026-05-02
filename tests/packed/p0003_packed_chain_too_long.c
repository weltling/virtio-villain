/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0003: packed_chain_too_long
 *
 * Create a packed descriptor chain longer than queue_size by using
 * NEXT flag on all descriptors including the last slot, which wraps
 * around to create an infinite chain.
 * Spec 2.8.17: descriptor list length MUST NOT exceed Queue Size.
 */
#include "tests/test.h"
#include "lib/util.h"
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

static test_result_t test_packed_chain_too_long(struct virtio_dev *dev,
                                                struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    uint16_t qsz = vr->size;

    /* Fill all slots with NEXT flag - creates chain of qsz descriptors */
    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);

    for (uint16_t i = 1; i < qsz - 1; i++) {
        vring_packed_set_desc(vr, i, data_phys, 512, i,
                              VRING_PACKED_DESC_F_NEXT |
                              VRING_PACKED_DESC_F_WRITE);
        vring_packed_advance(vr);
    }

    /* Last slot also has NEXT - chain wraps, exceeding qsz */
    vring_packed_set_desc(vr, qsz - 1, data_phys, 1, qsz - 1,
                          VRING_PACKED_DESC_F_NEXT |
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 0, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0003, VIRTIO_PCI_DEVICE_BLK, test_packed_chain_too_long,
                     "Packed descriptor list longer than Queue Size",
                     VIRTIO_SPEC_V1_2, "2.8.17");
