/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0025: packed_used_out_of_order
 *
 * Submit two independent requests at consecutive slots. The spec
 * allows the device to complete them out of order with packed rings.
 * Verify both complete regardless of order.
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

static test_result_t test_packed_used_out_of_order(struct virtio_dev *dev,
                                                   struct vring_packed *vr)
{
    if (vr->size < 8)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr0 = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *st0 = vv_alloc_pages(1);

    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *st1 = vv_alloc_pages(1);

    hdr0->type = VIRTIO_BLK_T_IN;
    hdr0->ioprio = 0;
    hdr0->sector = 0;
    *st0 = 0xFF;

    hdr1->type = VIRTIO_BLK_T_IN;
    hdr1->ioprio = 0;
    hdr1->sector = 1;
    *st1 = 0xFF;

    /* Build indirect tables for each request */
    struct vring_packed_desc *ind0 = vv_alloc_pages(1);
    ind0[0].addr = vv_virt_to_phys(hdr0);
    ind0[0].len = sizeof(*hdr0);
    ind0[0].id = 0;
    ind0[0].flags = VRING_PACKED_DESC_F_NEXT;
    ind0[1].addr = vv_virt_to_phys(data0);
    ind0[1].len = 512;
    ind0[1].id = 0;
    ind0[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
    ind0[2].addr = vv_virt_to_phys(st0);
    ind0[2].len = 1;
    ind0[2].id = 0;
    ind0[2].flags = VRING_PACKED_DESC_F_WRITE;

    struct vring_packed_desc *ind1 = vv_alloc_pages(1);
    ind1[0].addr = vv_virt_to_phys(hdr1);
    ind1[0].len = sizeof(*hdr1);
    ind1[0].id = 1;
    ind1[0].flags = VRING_PACKED_DESC_F_NEXT;
    ind1[1].addr = vv_virt_to_phys(data1);
    ind1[1].len = 512;
    ind1[1].id = 1;
    ind1[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
    ind1[2].addr = vv_virt_to_phys(st1);
    ind1[2].len = 1;
    ind1[2].id = 1;
    ind1[2].flags = VRING_PACKED_DESC_F_WRITE;

    /* Submit request 0 at slot 0 */
    uint8_t wrap0 = vr->wrap_counter;
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(ind0),
                          3 * sizeof(struct vring_packed_desc), 0,
                          VRING_PACKED_DESC_F_INDIRECT);
    uint16_t slot0 = vr->next_avail;
    vring_packed_advance(vr);

    /* Submit request 1 at slot 1 */
    uint8_t wrap1 = vr->wrap_counter;
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(ind1),
                          3 * sizeof(struct vring_packed_desc), 1,
                          VRING_PACKED_DESC_F_INDIRECT);
    uint16_t slot1 = vr->next_avail;
    vring_packed_advance(vr);

    /* Kick once for both */
    virtio_pci_kick(dev, vr->queue);

    /* Wait for both to be used (in any order) */
    int elapsed = 0;
    int done0 = 0, done1 = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        if (!done0 && vring_packed_desc_is_used(vr, slot0, wrap0))
            done0 = 1;
        if (!done1 && vring_packed_desc_is_used(vr, slot1, wrap1))
            done1 = 1;
        if (done0 && done1)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0025, VIRTIO_PCI_DEVICE_BLK, test_packed_used_out_of_order,
                     "Packed ring out-of-order used completion",
                     VIRTIO_SPEC_V1_2, "2.8.6");
