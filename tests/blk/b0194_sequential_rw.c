/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0194: sequential write then read across four sectors.
 *
 * Write different patterns to sectors 0, 1, 2, 3 sequentially,
 * then read all four back and verify each has its own pattern.
 * Tests sequential I/O correctness across a small range.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_seq_rw(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 4) return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    uint16_t avail_idx = 0;

    /* Write sectors 0..3 */
    for (int s = 0; s < 4; s++) {
        hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = s;
        memset(data, (uint8_t)(0x10 + s), 512); *st = 0xFF;
        vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                           VRING_DESC_F_NEXT, 2);
        vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, avail_idx, 0);
        vring_raw_set_avail_idx(vr, avail_idx + 1);
        test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
        if (r != TEST_PASS) return r;
        if (*st != VIRTIO_BLK_S_OK) TFAIL("write sector %d status %u", s, *st);
        avail_idx++;
    }

    /* Read and verify sectors 0..3 */
    for (int s = 0; s < 4; s++) {
        hdr->type = VIRTIO_BLK_T_IN; hdr->sector = s;
        memset(data, 0, 512); *st = 0xFF;
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
        vring_raw_set_avail(vr, avail_idx, 0);
        vring_raw_set_avail_idx(vr, avail_idx + 1);
        test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
        if (r != TEST_PASS) return r;
        if (*st != VIRTIO_BLK_S_OK) TFAIL("read sector %d status %u", s, *st);
        uint8_t expected = (uint8_t)(0x10 + s);
        for (int i = 0; i < 512; i++)
            if (data[i] != expected)
                TFAIL("sector %d byte %d: 0x%02x != 0x%02x",
                      s, i, data[i], expected);
        avail_idx++;
    }
    return TEST_PASS;
}

REGISTER_TEST(B0194, VIRTIO_PCI_DEVICE_BLK, test_blk_seq_rw,
              "Sequential write/read across four sectors",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
