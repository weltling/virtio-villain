/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0193: write then read the last valid sector.
 *
 * Write a pattern to sector (capacity - 1) and read it back.
 * Tests that the device handles the maximum valid sector offset
 * correctly at the boundary edge.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_last_sector_rw(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;

    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    uint64_t cap = bcfg->capacity;
    if (cap < 2) return TEST_SKIP;
    uint64_t last = cap - 1;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    /* Write */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = last;
    memset(data, 0xEE, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write status %u", *st);

    /* Read */
    hdr->type = VIRTIO_BLK_T_IN; memset(data, 0, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    for (int i = 0; i < 512; i++)
        if (data[i] != 0xEE)
            TFAIL("byte %d: 0x%02x != 0xEE", i, data[i]);

    return TEST_PASS;
}

REGISTER_TEST(B0193, VIRTIO_PCI_DEVICE_BLK, test_blk_last_sector_rw,
              "Write and read the last valid sector",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
