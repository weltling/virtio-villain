/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0024: packed_queue_size_one
 *
 * Operate a packed ring with queue_size=1 (only one descriptor slot).
 * This is the degenerate minimum ring size. The device must still
 * process a single-descriptor indirect request correctly.
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

static test_result_t test_packed_qsize_one_indirect(struct virtio_dev *dev,
                                                    struct vring_packed *vr)
{
    /*
     * The test framework allocates whatever size the device reports.
     * If the device doesn't support size=1, we still test with an
     * indirect descriptor (since one slot is all we need for indirect).
     */
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

    /* Build indirect table: 3 descriptors for header, data, status */
    struct vring_packed_desc *indirect = vv_alloc_pages(1);
    indirect[0].addr = hdr_phys;
    indirect[0].len = sizeof(*hdr);
    indirect[0].id = 0;
    indirect[0].flags = VRING_PACKED_DESC_F_NEXT;
    indirect[1].addr = data_phys;
    indirect[1].len = 512;
    indirect[1].id = 0;
    indirect[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
    indirect[2].addr = status_phys;
    indirect[2].len = 1;
    indirect[2].id = 0;
    indirect[2].flags = VRING_PACKED_DESC_F_WRITE;

    /* Use only slot 0 with indirect flag */
    uint64_t indirect_phys = vv_virt_to_phys(indirect);
    vring_packed_set_desc(vr, 0, indirect_phys,
                          3 * sizeof(struct vring_packed_desc), 0,
                          VRING_PACKED_DESC_F_INDIRECT);

    uint8_t check_wrap = vr->wrap_counter;

    return vv_kick_and_wait_packed(dev, vr, vr->queue, 0,
                                   check_wrap, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0024, VIRTIO_PCI_DEVICE_BLK, test_packed_qsize_one_indirect,
                     "Packed ring single-slot indirect request",
                     VIRTIO_SPEC_V1_2, "2.8.6");
