/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0052: batch completion signalling.
 *
 * v1.4 2.8.5: the device may signal completion only on the
 * head of each chain rather than every descriptor. Submit two
 * single descriptor BLK_IN chains back to back and verify
 * both heads become used.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>
#include <unistd.h>

struct blk_outhdr { uint32_t type; uint32_t ioprio; uint64_t sector; }
    __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static int submit(struct virtio_dev *dev, struct vring_packed *vr,
                  uint64_t sector, uint16_t *head_out, uint8_t *wrap_out)
{
    (void)dev;
    struct blk_outhdr *h = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    h->type = VIRTIO_BLK_T_IN; h->ioprio = 0; h->sector = sector;
    *status = 0xFF;

    uint16_t head = vr->next_avail;
    uint8_t wrap = vr->wrap_counter;
    *head_out = head; *wrap_out = wrap;

    vring_packed_set_desc(vr, head, vv_virt_to_phys(h), sizeof(*h),
                          head, VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    uint16_t mid = vr->next_avail;
    vring_packed_set_desc(vr, mid, vv_virt_to_phys(data), 512, head,
                          VRING_PACKED_DESC_F_NEXT |
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    uint16_t tail = vr->next_avail;
    vring_packed_set_desc(vr, tail, vv_virt_to_phys(status), 1, head,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    return 0;
}

static test_result_t test(struct virtio_dev *dev, struct vring_packed *vr)
{
    if (vr->size < 8) return TEST_SKIP;
    uint16_t h1, h2; uint8_t w1, w2;
    submit(dev, vr, 0, &h1, &w1);
    submit(dev, vr, 1, &h2, &w2);
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        if (vring_packed_desc_is_used(vr, h1, w1) &&
            vring_packed_desc_is_used(vr, h2, w2))
            return TEST_PASS;
        usleep(10000); elapsed += 10000;
    }
    return TEST_REJECT;
}

REGISTER_TEST_PACKED(P0052, VIRTIO_PCI_DEVICE_BLK, test,
                     "Batch of two chains completes both heads",
                     VIRTIO_SPEC_V1_4, "2.8.5");
