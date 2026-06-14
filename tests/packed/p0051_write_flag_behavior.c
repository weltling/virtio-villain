/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0051: F_WRITE flag interpretation on packed chains.
 *
 * v1.4 2.8.4: in a packed chain F_WRITE marks the descriptor
 * as device writable. Submit a BLK_IN with explicit F_WRITE on
 * the data descriptor and verify the device fills it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test(struct virtio_dev *dev, struct vring_packed *vr)
{
    struct virtio_blk_outhdr *h = vv_alloc_pages(1);
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

REGISTER_TEST_PACKED(P0051, VIRTIO_PCI_DEVICE_BLK, test,
                     "F_WRITE flag on packed data descriptor",
                     VIRTIO_SPEC_V1_4, "2.8.4");
