/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0012: packed_invalid_buffer_id
 *
 * Submit a packed descriptor with id >= queue_size. The buffer ID must
 * be in [0, queue_size-1]. An out-of-range ID can cause the device to
 * index out of bounds in its internal buffer tracking array.
 */
#include "tests/test.h"
#include "lib/util.h"
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

static test_result_t test_packed_invalid_buffer_id(struct virtio_dev *dev,
                                                   struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Use id = queue_size (OOB) for the first descriptor */
    uint16_t bad_id = vr->size; /* 16 when queue is 16 */

    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), bad_id,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, data_phys, 512, bad_id,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, status_phys, 1, bad_id,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 2, vr->wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0012, VIRTIO_PCI_DEVICE_BLK, test_packed_invalid_buffer_id,
                     "Packed descriptor with id >= queue_size",
                     VIRTIO_SPEC_V1_2, "2.8.6");
