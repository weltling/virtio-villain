/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0104: Write followed by read of same sector (ordering test)
 *
 * Submit WRITE then READ to the same sector in one batch.
 * Tests whether device processes them in order.
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

#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

static test_result_t test_blk_write_read_same_sector(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr_w = vv_alloc_pages(1);
    uint8_t *data_w = vv_alloc_pages(1);
    uint8_t *status_w = vv_alloc_pages(1);

    struct virtio_blk_outhdr *hdr_r = vv_alloc_pages(1);
    uint8_t *data_r = vv_alloc_pages(1);
    uint8_t *status_r = vv_alloc_pages(1);

    hdr_w->type = VIRTIO_BLK_T_OUT;
    hdr_w->ioprio = 0;
    hdr_w->sector = 0;
    memset(data_w, 0x42, 512);
    *status_w = 0xFF;

    hdr_r->type = VIRTIO_BLK_T_IN;
    hdr_r->ioprio = 0;
    hdr_r->sector = 0;
    *status_r = 0xFF;

    /* Chain 1 (write): descs 0-1-2 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr_w), sizeof(*hdr_w),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data_w), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status_w), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Chain 2 (read): descs 3-4-5 */
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(hdr_r), sizeof(*hdr_r),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(data_r), 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, vv_virt_to_phys(status_r), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0104, VIRTIO_PCI_DEVICE_BLK, test_blk_write_read_same_sector,
              "Write then read same sector in single batch",
              VIRTIO_SPEC_V1_2, "5.2.6");
