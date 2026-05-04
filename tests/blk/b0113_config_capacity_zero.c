/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0113: blk_config_capacity_read
 *
 * Read capacity from device config space, then submit a read to
 * sector 0 when capacity is reported as nonzero. Baseline correctness
 * test for config space access and normal read path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

struct virtio_blk_config {
    uint64_t capacity;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_blk_config_capacity(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* Read capacity from device-specific config */
    volatile struct virtio_blk_config *blkcfg =
        (volatile struct virtio_blk_config *)dev->device_cfg;

    __sync_synchronize();
    uint64_t capacity = blkcfg->capacity;

    if (capacity == 0)
        return TEST_SKIP;

    /* Submit a basic read to sector 0 */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0113, VIRTIO_PCI_DEVICE_BLK, test_blk_config_capacity,
              "Read config capacity then read sector 0",
              VIRTIO_SPEC_V1_2, "5.2.5.1");
