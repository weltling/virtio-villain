/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0012: packed_invalid_buffer_id
 *
 * Submit a packed descriptor with id >= queue_size. The buffer ID must
 * be in [0, queue_size-1]. An out-of-range ID can cause the device to
 * index out of bounds in its internal buffer tracking array.
 */
#include "lib/virtio_test.h"

static test_result_t test_packed_invalid_buffer_id(struct virtio_dev *dev,
                                                   struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Use id = queue_size (OOB) for the first descriptor */
    uint16_t bad_id = vr->size; /* 16 when queue is 16 */

    vring_packed_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr), bad_id,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, vv_virt_to_phys(data), 512, bad_id,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, vv_virt_to_phys(status), 1, bad_id,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 2, vr->wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0012, VIRTIO_PCI_DEVICE_BLK, test_packed_invalid_buffer_id,
                     "Packed descriptor with id >= queue_size",
                     VIRTIO_SPEC_V1_2, "2.8.6");
