/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0192: write/read different sectors are independent.
 *
 * Write 0xAA to sector 0, write 0xBB to sector 1, then read both
 * back and verify each has its own pattern. Tests that sector
 * addressing correctly isolates data across different LBAs.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_sector_isolation(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 2)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    /* Write 0xAA to sector 0 */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    memset(data, 0xAA, 512); *st = 0xFF;
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

    /* Write 0xBB to sector 1 */
    hdr->sector = 1; memset(data, 0xBB, 512); *st = 0xFF;
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Read sector 0 */
    hdr->type = VIRTIO_BLK_T_IN; hdr->sector = 0;
    memset(data, 0, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_avail(vr, 2, 0);
    vring_raw_set_avail_idx(vr, 3);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    for (int i = 0; i < 512; i++)
        if (data[i] != 0xAA)
            TFAIL("sector0 byte %d: 0x%02x != 0xAA", i, data[i]);

    /* Read sector 1 */
    hdr->sector = 1; memset(data, 0, 512); *st = 0xFF;
    vring_raw_set_avail(vr, 3, 0);
    vring_raw_set_avail_idx(vr, 4);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    for (int i = 0; i < 512; i++)
        if (data[i] != 0xBB)
            TFAIL("sector1 byte %d: 0x%02x != 0xBB", i, data[i]);

    return TEST_PASS;
}

REGISTER_TEST(B0192, VIRTIO_PCI_DEVICE_BLK, test_blk_sector_isolation,
              "Different sectors store independent data",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
