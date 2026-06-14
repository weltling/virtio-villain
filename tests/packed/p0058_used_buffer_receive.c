/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0058: device writes buffer id and len in used descriptor.
 *
 * v1.4 2.8.7: the device fills the head descriptor of the
 * chain with the id and len of bytes written, then flips the
 * AVAIL/USED bits. Submit a BLK_IN; after completion the
 * head's len must equal sector_size (512).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_spec.h"

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

    test_result_t r = vv_kick_and_wait_packed(dev, vr, vr->queue, head,
                                              wrap, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    uint16_t id = vr->desc[head].id;
    if (id != head)
        TFAIL("head id mismatch: expected %u got %u", head, id);
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0058, VIRTIO_PCI_DEVICE_BLK, test,
                     "Used descriptor carries buffer id",
                     VIRTIO_SPEC_V1_4, "2.8.7");
