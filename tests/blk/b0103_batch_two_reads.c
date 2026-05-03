/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0103: Multiple requests in a single kick (batch submission)
 *
 * Submit two independent READ requests at once by placing two
 * descriptor chains in the avail ring before a single kick.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_blk_batch_two_reads(struct virtio_dev *dev,
                                              struct vring *vr)
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

    /* Chain 1: descs 0-1-2 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data1), 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status1), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Chain 2: descs 3-4-5 */
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(hdr2), sizeof(*hdr2),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(data2), 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, vv_virt_to_phys(status2), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Both chains in avail ring */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0103, VIRTIO_PCI_DEVICE_BLK, test_blk_batch_two_reads,
              "Batch: two READ requests submitted with single kick",
              VIRTIO_SPEC_V1_2, "5.2.6");
