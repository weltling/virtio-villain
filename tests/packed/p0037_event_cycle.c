/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0037: cycle driver_event flags between batches
 *
 * Spec 2.8.10 lets the driver toggle event suppression between
 * ENABLE, DISABLE and DESC at any time. Submit three small batches
 * back to back, switching driver_event->flags between every batch
 * (ENABLE then DISABLE then ENABLE). The device must process every
 * batch regardless of suppression state since suppression only
 * affects whether a notification is raised, not whether work runs.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define RING_EVENT_FLAGS_ENABLE  0
#define RING_EVENT_FLAGS_DISABLE 1

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static int submit_one(struct virtio_dev *dev, struct vring_packed *vr,
                      uint64_t sector)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = sector;
    *status = 0xFF;

    uint16_t head = vr->next_avail;
    uint8_t check_wrap = vr->wrap_counter;

    vring_packed_set_desc(vr, head, vv_virt_to_phys(hdr), sizeof(*hdr),
                          head, VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    uint16_t mid = vr->next_avail;
    vring_packed_set_desc(vr, mid, vv_virt_to_phys(data), 512,
                          head, VRING_PACKED_DESC_F_NEXT |
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    uint16_t tail = vr->next_avail;
    vring_packed_set_desc(vr, tail, vv_virt_to_phys(status), 1,
                          head, VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        if (vring_packed_desc_is_used(vr, head, check_wrap))
            return 0;
        elapsed += 10000;
    }
    return -1;
}

static test_result_t test_packed_event_cycle(struct virtio_dev *dev,
                                             struct vring_packed *vr)
{
    if (vr->size < 12)
        return TEST_SKIP;

    uint16_t modes[3] = {
        RING_EVENT_FLAGS_ENABLE,
        RING_EVENT_FLAGS_DISABLE,
        RING_EVENT_FLAGS_ENABLE,
    };

    for (int i = 0; i < 3; i++) {
        vr->driver_event->flags = modes[i];
        vr->driver_event->off_wrap = 0;
        __sync_synchronize();
        if (submit_one(dev, vr, (uint64_t)i) < 0)
            TREJECT("submit_one(dev, vr, (uint64_t)i) < 0");
    }
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0037, VIRTIO_PCI_DEVICE_BLK, test_packed_event_cycle,
                     "Cycle driver_event flags between batches",
                     VIRTIO_SPEC_V1_2, "2.8.10");
