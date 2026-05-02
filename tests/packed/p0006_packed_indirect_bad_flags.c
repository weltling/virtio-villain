/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0006: packed_indirect_bad_flags
 *
 * In an indirect descriptor table, set flags other than WRITE and NEXT
 * (specifically, set the AVAIL and USED bits which are meaningless
 * inside indirect tables).
 * Spec 2.8.19: within indirect table, only WRITE and NEXT flags are
 * valid.
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

struct indirect_packed_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t id;
    uint16_t flags;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_packed_indirect_bad_flags(struct virtio_dev *dev,
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

    /* Build indirect table with illegal AVAIL/USED bits set */
    struct indirect_packed_desc *ind = vv_alloc_pages(1);
    ind[0].addr = hdr_phys;
    ind[0].len = sizeof(*hdr);
    ind[0].id = 0;
    ind[0].flags = VRING_PACKED_DESC_F_NEXT |
                   VRING_PACKED_DESC_F_AVAIL | VRING_PACKED_DESC_F_USED;
    ind[1].addr = data_phys;
    ind[1].len = 512;
    ind[1].id = 1;
    ind[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE |
                   VRING_PACKED_DESC_F_AVAIL;
    ind[2].addr = status_phys;
    ind[2].len = 1;
    ind[2].id = 2;
    ind[2].flags = VRING_PACKED_DESC_F_WRITE | VRING_PACKED_DESC_F_USED;

    uint64_t ind_phys = vv_virt_to_phys(ind);

    vring_packed_set_desc(vr, 0, ind_phys,
                          3 * sizeof(struct indirect_packed_desc), 0,
                          VRING_PACKED_DESC_F_INDIRECT);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 0, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0006, VIRTIO_PCI_DEVICE_BLK, test_packed_indirect_bad_flags,
                     "AVAIL/USED flags set in indirect descriptor",
                     VIRTIO_SPEC_V1_2, "2.8.19");
