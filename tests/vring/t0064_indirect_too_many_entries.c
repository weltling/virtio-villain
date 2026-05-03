/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0064: indirect_too_many_entries
 *
 * Create an indirect descriptor table with more entries than queue_size.
 * The spec says the table length must not exceed queue_size entries.
 * Tests that the device enforces this limit.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

struct vring_desc_raw {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_indirect_too_many(struct virtio_dev *dev,
                                            struct vring *vr)
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
     * Build indirect table with 128 entries (queue_size is 64).
     * Only the first 3 are meaningful, rest are padding.
     */
    struct vring_desc_raw *indirect = vv_alloc_pages(4);
    uint16_t num_entries = 128; /* > queue_size */

    indirect[0].addr = hdr_phys;
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;

    indirect[1].addr = data_phys;
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    indirect[1].next = 2;

    indirect[2].addr = status_phys;
    indirect[2].len = 1;
    indirect[2].flags = VRING_DESC_F_WRITE;
    indirect[2].next = 0;

    /* Fill remaining with zeros */
    memset(&indirect[3], 0, (num_entries - 3) * sizeof(struct vring_desc_raw));

    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    /* Point ring descriptor at indirect table with oversized len */
    vring_raw_set_desc(vr, 0, indirect_phys,
                       num_entries * sizeof(struct vring_desc_raw),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0064, VIRTIO_PCI_DEVICE_BLK, test_indirect_too_many,
              "Indirect table with more entries than queue_size",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
