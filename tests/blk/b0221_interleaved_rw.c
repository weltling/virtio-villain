/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0221: interleaved writes to two sectors then verify both.
 *
 * Write pattern A to sector 0, write pattern B to sector 1, then
 * read sector 1 first and sector 0 second. Verify each sector
 * retained its own pattern regardless of read order.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_interleave(struct virtio_dev *dev,
                                         struct vring *vr)
{
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 2) return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    uint16_t ai = 0;

    /* Write 0x11 to sector 0 */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    memset(data, 0x11, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Write 0x22 to sector 1 */
    hdr->sector = 1; memset(data, 0x22, 512); *st = 0xFF;
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Read sector 1 first (reverse order) */
    hdr->type = VIRTIO_BLK_T_IN; hdr->sector = 1;
    memset(data, 0, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    for (int i = 0; i < 512; i++)
        if (data[i] != 0x22)
            TFAIL("sector1 byte %d: 0x%02x != 0x22", i, data[i]);

    /* Read sector 0 */
    hdr->sector = 0; memset(data, 0, 512); *st = 0xFF;
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    for (int i = 0; i < 512; i++)
        if (data[i] != 0x11)
            TFAIL("sector0 byte %d: 0x%02x != 0x11", i, data[i]);

    return TEST_PASS;
}

REGISTER_TEST(B0221, VIRTIO_PCI_DEVICE_BLK, test_blk_interleave,
              "Interleaved writes then reverse order reads",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
