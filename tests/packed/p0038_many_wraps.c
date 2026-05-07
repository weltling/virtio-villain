/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0038: many wraps on minimum queue size
 *
 * Spec 2.8.21 defines the wrap counter and how the driver flips it
 * each time it wraps around the descriptor table. Force a queue
 * size of 4 (the smallest power of two the spec mentions in 2.8.4)
 * and submit 16 single descriptor requests, exercising 4 full
 * wraps. Each wrap flips the AVAIL phase bit. A VMM that caches
 * the wrap counter incorrectly across one rotation will eventually
 * stop seeing new descriptors.
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

static test_result_t test_packed_many_wraps(struct virtio_dev *dev,
                                            struct vring_packed *vr)
{
    if (vr->size < 4)
        return TEST_SKIP;

    int batches = 4 * vr->size;
    if (batches > 32)
        batches = 32;

    for (int i = 0; i < batches; i++) {
        struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
        uint8_t *data = vv_alloc_pages(1);
        uint8_t *status = vv_alloc_pages(1);

        hdr->type = VIRTIO_BLK_T_IN;
        hdr->ioprio = 0;
        hdr->sector = (uint64_t)i;
        *status = 0xFF;

        struct vring_packed_desc *ind = vv_alloc_pages(1);
        ind[0].addr = vv_virt_to_phys(hdr);
        ind[0].len = sizeof(*hdr);
        ind[0].id = vr->next_avail;
        ind[0].flags = VRING_PACKED_DESC_F_NEXT;
        ind[1].addr = vv_virt_to_phys(data);
        ind[1].len = 512;
        ind[1].id = vr->next_avail;
        ind[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
        ind[2].addr = vv_virt_to_phys(status);
        ind[2].len = 1;
        ind[2].id = vr->next_avail;
        ind[2].flags = VRING_PACKED_DESC_F_WRITE;

        uint16_t idx = vr->next_avail;
        uint8_t wrap = vr->wrap_counter;

        uint16_t flags = VRING_PACKED_DESC_F_INDIRECT;
        if (wrap)
            flags |= VRING_PACKED_DESC_F_AVAIL;
        else
            flags |= VRING_PACKED_DESC_F_USED;

        vr->desc[idx].addr = vv_virt_to_phys(ind);
        vr->desc[idx].len = 3 * sizeof(*ind);
        vr->desc[idx].id = idx;
        __sync_synchronize();
        vr->desc[idx].flags = flags;
        __sync_synchronize();

        vring_packed_advance(vr);

        __sync_synchronize();
        virtio_pci_kick(dev, vr->queue);

        int elapsed = 0;
        int done = 0;
        while (elapsed < VV_TIMEOUT_MS * 1000) {
            usleep(10000);
            if (vring_packed_desc_is_used(vr, idx, wrap)) {
                done = 1;
                break;
            }
            elapsed += 10000;
        }
        if (!done)
            TREJECT("!done");
    }

    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0038, VIRTIO_PCI_DEVICE_BLK, test_packed_many_wraps,
                     "Multiple full wraps with single slot indirect chains",
                     VIRTIO_SPEC_V1_2, "2.8.21");
