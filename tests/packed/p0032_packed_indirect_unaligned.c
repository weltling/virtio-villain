/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0032: Submit a packed indirect descriptor whose table address is
 * not 16-byte aligned.
 *
 * Spec 2.8.7: The indirect descriptor table must be 16-byte aligned.
 * We set addr to a misaligned offset and verify the device handles
 * it without crashing.
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

static test_result_t test_packed_indirect_unaligned(struct virtio_dev *dev,
                                                    struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Build indirect table at a properly aligned location */
    uint8_t *table_page = vv_alloc_pages(1);
    struct vring_packed_desc *indirect =
        (struct vring_packed_desc *)(table_page + 5); /* misaligned by 5 */

    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].id = 0;
    indirect[0].flags = VRING_PACKED_DESC_F_NEXT;

    indirect[1].addr = vv_virt_to_phys(data);
    indirect[1].len = 512;
    indirect[1].id = 0;
    indirect[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;

    indirect[2].addr = vv_virt_to_phys(status);
    indirect[2].len = 1;
    indirect[2].id = 0;
    indirect[2].flags = VRING_PACKED_DESC_F_WRITE;

    /* Submit indirect descriptor with misaligned table address */
    uint64_t indirect_phys = vv_virt_to_phys(indirect);
    uint8_t wrap = vr->wrap_counter;

    vring_packed_set_desc(vr, vr->next_avail, indirect_phys,
                          3 * sizeof(struct vring_packed_desc), 0,
                          VRING_PACKED_DESC_F_INDIRECT);

    uint16_t check_idx = vr->next_avail;
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, check_idx, wrap,
                                   VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0032, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_unaligned,
                     "Packed indirect table at non-16-byte aligned address",
                     VIRTIO_SPEC_V1_2, "2.8.7");
