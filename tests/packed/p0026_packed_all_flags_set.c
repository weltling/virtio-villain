/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0026: All descriptor flag bits set (spec 2.8.6)
 *
 * Set all flag bits (including reserved ones) on a packed descriptor.
 * The device must handle reserved bits gracefully.
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

static test_result_t test_packed_all_flags(struct virtio_dev *dev,
                                           struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    /* Build indirect table for the request */
    struct vring_packed_desc *ind = vv_alloc_pages(1);
    ind[0].addr = vv_virt_to_phys(hdr);
    ind[0].len = sizeof(*hdr);
    ind[0].id = 0;
    ind[0].flags = VRING_PACKED_DESC_F_NEXT;
    ind[1].addr = vv_virt_to_phys(data);
    ind[1].len = 512;
    ind[1].id = 0;
    ind[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
    ind[2].addr = vv_virt_to_phys(st);
    ind[2].len = 1;
    ind[2].id = 0;
    ind[2].flags = VRING_PACKED_DESC_F_WRITE;

    /* Submit with ALL flag bits set on the ring descriptor (0xFFFF) */
    uint16_t idx = vr->next_avail;
    vr->desc[idx].addr = vv_virt_to_phys(ind);
    vr->desc[idx].len = 3 * sizeof(*ind);
    vr->desc[idx].id = 0;
    /* All bits: NEXT|WRITE|INDIRECT|AVAIL|USED + reserved */
    vr->desc[idx].flags = 0xFFFF;
    __sync_synchronize();

    vr->next_avail = (idx + 1) % vr->size;

    /* Kick */
    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    /* Check if device is alive */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0026, VIRTIO_PCI_DEVICE_BLK, test_packed_all_flags,
    "Packed descriptor with all flag bits (0xFFFF) set",
    VIRTIO_SPEC_V1_2, "2.8.6");
