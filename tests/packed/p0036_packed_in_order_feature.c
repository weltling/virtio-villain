/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0036: Submit multiple packed descriptors and verify completion order
 * when VIRTIO_F_IN_ORDER is not negotiated.
 *
 * Spec 2.8.10: Without VIRTIO_F_IN_ORDER, the device MAY complete
 * descriptors out of order. Submit several requests and verify
 * all complete regardless of order.
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
#define NUM_REQUESTS 4

static test_result_t test_packed_in_order_feature(struct virtio_dev *dev,
                                                  struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdrs[NUM_REQUESTS];
    uint8_t *datas[NUM_REQUESTS];
    uint8_t *sts[NUM_REQUESTS];
    uint16_t chain_start[NUM_REQUESTS];

    for (int i = 0; i < NUM_REQUESTS; i++) {
        hdrs[i] = vv_alloc_pages(1);
        datas[i] = vv_alloc_pages(1);
        sts[i] = vv_alloc_pages(1);

        hdrs[i]->type = VIRTIO_BLK_T_IN;
        hdrs[i]->ioprio = 0;
        hdrs[i]->sector = (uint64_t)i;
        *sts[i] = 0xFF;
    }

    uint8_t first_wrap = vr->wrap_counter;

    /* Submit NUM_REQUESTS chains of 3 descriptors each */
    for (int i = 0; i < NUM_REQUESTS; i++) {
        chain_start[i] = vr->next_avail;

        vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(hdrs[i]),
                              sizeof(*hdrs[i]), (uint16_t)i,
                              VRING_PACKED_DESC_F_NEXT);
        vring_packed_advance(vr);

        vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(datas[i]),
                              512, (uint16_t)i,
                              VRING_PACKED_DESC_F_NEXT |
                              VRING_PACKED_DESC_F_WRITE);
        vring_packed_advance(vr);

        vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(sts[i]),
                              1, (uint16_t)i, VRING_PACKED_DESC_F_WRITE);
        vring_packed_advance(vr);
    }

    /* Single kick for all requests */
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Wait for all to complete */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        int done = 0;
        for (int i = 0; i < NUM_REQUESTS; i++) {
            if (vring_packed_desc_is_used(vr, chain_start[i], first_wrap))
                done++;
        }
        if (done == NUM_REQUESTS)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0036, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_in_order_feature,
                     "Packed completion order without VIRTIO_F_IN_ORDER",
                     VIRTIO_SPEC_V1_2, "2.8.10");
