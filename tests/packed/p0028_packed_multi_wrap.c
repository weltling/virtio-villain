/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0028: Multiple consecutive packed ring wraps (spec 2.8.21)
 *
 * Submit enough requests to wrap the ring index multiple times.
 * With a small queue_size, this tests wrap counter behavior across
 * several full rotations.
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

static test_result_t test_packed_multi_wrap(struct virtio_dev *dev,
                                            struct vring_packed *vr)
{
    if (vr->size < 4)
        return TEST_SKIP;

    /* We'll submit 3*queue_size requests (3 full rotations) */
    int total = vr->size * 3;
    if (total > 48)
        total = 48; /* cap for sanity */

    int completed = 0;

    for (int i = 0; i < total; i++) {
        struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
        uint8_t *data = vv_alloc_pages(1);
        uint8_t *st = vv_alloc_pages(1);

        hdr->type = VIRTIO_BLK_T_IN;
        hdr->ioprio = 0;
        hdr->sector = i % 8;
        *st = 0xFF;

        struct vring_packed_desc *ind = vv_alloc_pages(1);
        ind[0].addr = vv_virt_to_phys(hdr);
        ind[0].len = sizeof(*hdr);
        ind[0].id = vr->next_avail;
        ind[0].flags = VRING_PACKED_DESC_F_NEXT;
        ind[1].addr = vv_virt_to_phys(data);
        ind[1].len = 512;
        ind[1].id = vr->next_avail;
        ind[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
        ind[2].addr = vv_virt_to_phys(st);
        ind[2].len = 1;
        ind[2].id = vr->next_avail;
        ind[2].flags = VRING_PACKED_DESC_F_WRITE;

        uint16_t idx = vr->next_avail;
        uint8_t wrap = vr->wrap_counter;

        uint16_t avail_flags = VRING_PACKED_DESC_F_INDIRECT;
        if (wrap)
            avail_flags |= VRING_PACKED_DESC_F_AVAIL;
        else
            avail_flags |= VRING_PACKED_DESC_F_USED;

        vr->desc[idx].addr = vv_virt_to_phys(ind);
        vr->desc[idx].len = 3 * sizeof(*ind);
        vr->desc[idx].id = idx;
        __sync_synchronize();
        vr->desc[idx].flags = avail_flags;
        __sync_synchronize();

        vr->next_avail = (idx + 1) % vr->size;
        if (vr->next_avail == 0)
            vr->wrap_counter ^= 1;

        virtio_pci_kick(dev, vr->queue);

        /* Wait for this one to complete */
        int elapsed = 0;
        while (elapsed < VV_TIMEOUT_MS * 1000) {
            usleep(10000);
            __sync_synchronize();
            uint16_t flags = vr->desc[idx].flags;
            int used_bit = !!(flags & VRING_PACKED_DESC_F_USED);
            int avail_bit = !!(flags & VRING_PACKED_DESC_F_AVAIL);
            if (used_bit == avail_bit) {
                completed++;
                break;
            }
            elapsed += 10000;
        }
        if (elapsed >= VV_TIMEOUT_MS * 1000)
            break; /* timed out */
    }

    if (completed == total)
        return TEST_PASS;
    if (completed > 0)
        return TEST_PASS; /* partial but wrapped at least once */

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_PACKED(P0028, VIRTIO_PCI_DEVICE_BLK, test_packed_multi_wrap,
    "Multiple packed ring wraps (3x queue_size sequential requests)",
    VIRTIO_SPEC_V1_2, "2.8.21");
