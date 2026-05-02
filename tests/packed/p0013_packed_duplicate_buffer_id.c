/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0013: packed_duplicate_buffer_id
 *
 * Submit two separate descriptor chains simultaneously using the same
 * buffer ID. The device tracks in-flight buffers by ID; duplicate IDs
 * can cause double-completion, use-after-free, or confused ownership.
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

static test_result_t test_packed_duplicate_id(struct virtio_dev *dev,
                                              struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *hdr2 = vv_alloc_pages(1);
    uint8_t *data2 = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_IN;
    hdr1->ioprio = 0;
    hdr1->sector = 0;
    *status1 = 0xFF;

    hdr2->type = VIRTIO_BLK_T_IN;
    hdr2->ioprio = 0;
    hdr2->sector = 1;
    *status2 = 0xFF;

    /* Chain 1: descs 0-2 with id=0 */
    vring_packed_set_desc(vr, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, vv_virt_to_phys(data1), 512, 0,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, vv_virt_to_phys(status1), 1, 0,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    /* Chain 2: descs 3-5 with SAME id=0 (duplicate!) */
    vring_packed_set_desc(vr, 3, vv_virt_to_phys(hdr2), sizeof(*hdr2), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 4, vv_virt_to_phys(data2), 512, 0,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 5, vv_virt_to_phys(status2), 1, 0,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 5, vr->wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0013, VIRTIO_PCI_DEVICE_BLK, test_packed_duplicate_id,
                     "Two in-flight descriptors with same buffer id",
                     VIRTIO_SPEC_V1_2, "2.8.6");
