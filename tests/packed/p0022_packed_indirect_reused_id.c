/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0022: packed_indirect_reused_buffer_id
 *
 * Submit an indirect descriptor in packed mode using a buffer_id that
 * was previously used and "completed." Tests whether the device
 * correctly handles buffer_id recycling with indirect descriptors.
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

static test_result_t test_packed_indirect_reused_id(struct virtio_dev *dev,
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

    /* Build indirect table */
    struct vring_desc *indirect = vv_alloc_pages(1);
    indirect[0].addr = hdr_phys;
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = data_phys;
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    indirect[1].next = 2;
    indirect[2].addr = status_phys;
    indirect[2].len = 1;
    indirect[2].flags = VRING_DESC_F_WRITE;
    indirect[2].next = 0;

    uint64_t ind_phys = vv_virt_to_phys(indirect);

    /* First submission with buffer_id = 42 */
    vr->desc[0].addr = ind_phys;
    vr->desc[0].len = 3 * sizeof(struct vring_desc);
    vr->desc[0].id = 42;

    uint16_t avail_flag = vr->wrap_counter ? (1 << 7) : 0;
    uint16_t used_flag = vr->wrap_counter ? 0 : (1 << 15);
    __sync_synchronize();
    vr->desc[0].flags = VRING_PACKED_DESC_F_INDIRECT | avail_flag | used_flag;
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);

    /* Wait for first completion */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        if (vring_packed_desc_is_used(vr, 0, vr->wrap_counter))
            break;
        elapsed += 10000;
    }
    if (elapsed >= VV_TIMEOUT_MS * 1000) {
        volatile struct virtio_pci_common_cfg *cfg = dev->common;
        __sync_synchronize();
        if (cfg->device_status == 0)
            TWEDGED("cfg->device_status == 0");
        TREJECT("elapsed >= VV_TIMEOUT_MS * 1000");
    }

    /* Advance wrap state for slot 1 */
    uint16_t next_idx = 1;
    /* Reuse same buffer_id=42 for second submission */
    *status = 0xFF;

    vr->desc[next_idx].addr = ind_phys;
    vr->desc[next_idx].len = 3 * sizeof(struct vring_desc);
    vr->desc[next_idx].id = 42; /* reused! */
    __sync_synchronize();
    vr->desc[next_idx].flags = VRING_PACKED_DESC_F_INDIRECT |
                                avail_flag | used_flag;
    __sync_synchronize();

    return vv_kick_and_wait_packed(dev, vr, 0, next_idx,
                                   vr->wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0022, VIRTIO_PCI_DEVICE_BLK, test_packed_indirect_reused_id,
                     "Packed indirect with reused buffer_id from completed request",
                     VIRTIO_SPEC_V1_2, "2.8.6");
