/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0224: write and read 16384 bytes across four page descriptors.
 *
 * Write 16K of patterned data using one descriptor per physical
 * page, read it back the same way, and verify all bytes. Tests
 * a four segment scatter gather I/O with content verification.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_16k(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 32) return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *pg[4];
    for (int k = 0; k < 4; k++) pg[k] = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    /* Fill each page with a continuous pattern */
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < 4096; i++)
            pg[k][i] = (uint8_t)(((k * 4096 + i) * 5 + 11) & 0xFF);

    /* WRITE: hdr -> pg0 -> pg1 -> pg2 -> pg3 -> status */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    for (int k = 0; k < 4; k++)
        vring_raw_set_desc(vr, 1 + k, vv_virt_to_phys(pg[k]), 4096,
                           VRING_DESC_F_NEXT, 2 + k);
    vring_raw_set_desc(vr, 5, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write status %u", *st);

    /* READ back */
    for (int k = 0; k < 4; k++) memset(pg[k], 0, 4096);
    *st = 0xFF;
    hdr->type = VIRTIO_BLK_T_IN;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    for (int k = 0; k < 4; k++)
        vring_raw_set_desc(vr, 1 + k, vv_virt_to_phys(pg[k]), 4096,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2 + k);
    vring_raw_set_desc(vr, 5, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    for (int k = 0; k < 4; k++)
        for (int i = 0; i < 4096; i++) {
            uint8_t e = (uint8_t)(((k * 4096 + i) * 5 + 11) & 0xFF);
            if (pg[k][i] != e)
                TFAIL("pg%d byte %d: 0x%02x != 0x%02x", k, i, pg[k][i], e);
        }
    return TEST_PASS;
}

REGISTER_TEST(B0224, VIRTIO_PCI_DEVICE_BLK, test_blk_16k,
              "Write and read 16384 bytes across four page descriptors",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
