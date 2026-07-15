/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0180: read from non zero sector offset.
 *
 * Read sector 2 (byte offset 1024) instead of sector 0. Verify the
 * device handles sector offsets correctly and does not always return
 * data from sector 0. Write a pattern to sector 2, read it back,
 * and compare.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_read_sector_offset(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 4)
        return TEST_SKIP;

    /* Write to sector 2 */
    struct virtio_blk_outhdr *whdr = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);

    whdr->type = VIRTIO_BLK_T_OUT;
    whdr->ioprio = 0;
    whdr->sector = 2;
    for (int i = 0; i < 512; i++)
        wdata[i] = (uint8_t)(i ^ 0x77);
    *wst = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(whdr), sizeof(*whdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(wdata), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(wst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*wst != VIRTIO_BLK_S_OK) TFAIL("write status %u", *wst);

    /* Read sector 2 back */
    struct virtio_blk_outhdr *rhdr = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    rhdr->type = VIRTIO_BLK_T_IN;
    rhdr->ioprio = 0;
    rhdr->sector = 2;
    memset(rdata, 0, 512);
    *rst = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rhdr), sizeof(*rhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(rdata), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(rst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*rst != VIRTIO_BLK_S_OK) TFAIL("read status %u", *rst);

    for (int i = 0; i < 512; i++) {
        uint8_t e = (uint8_t)(i ^ 0x77);
        if (rdata[i] != e)
            TFAIL("byte %d: 0x%02x != 0x%02x", i, rdata[i], e);
    }
    return TEST_PASS;
}

REGISTER_TEST(B0180, VIRTIO_PCI_DEVICE_BLK, test_blk_read_sector_offset,
              "Write and read sector 2 to verify offset handling",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
