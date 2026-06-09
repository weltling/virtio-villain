/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0057: driver kick is required when device requests notify.
 *
 * v1.4 2.8.13: with driver suppression DISABLE the driver must
 * notify on every submission. Place a chain and kick; verify
 * the device processes it.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <unistd.h>

#define RING_EVENT_FLAGS_DISABLE 1

struct blk_outhdr { uint32_t type; uint32_t ioprio; uint64_t sector; }
    __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test(struct virtio_dev *dev, struct vring_packed *vr)
{
    if (vr->device_event)
        vr->device_event->flags = RING_EVENT_FLAGS_DISABLE;
    __sync_synchronize();

    struct blk_outhdr *h = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    h->type = VIRTIO_BLK_T_IN; h->ioprio = 0; h->sector = 0;
    *status = 0xFF;

    uint16_t head = vr->next_avail;
    uint8_t wrap = vr->wrap_counter;
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

    return vv_kick_and_wait_packed(dev, vr, vr->queue, head, wrap,
                                   VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0057, VIRTIO_PCI_DEVICE_BLK, test,
                     "Kick after disabling device suppression",
                     VIRTIO_SPEC_V1_4, "2.8.13");
