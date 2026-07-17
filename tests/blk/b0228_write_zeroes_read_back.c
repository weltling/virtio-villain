/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0228: write zeroes then read returns zeros.
 *
 * Write a nonzero pattern to sector 0, issue WRITE_ZEROES for that
 * sector, then read it back and verify every byte is zero. Tests
 * that WRITE_ZEROES actually clears the referenced sector rather
 * than being silently ignored (spec 5.2.6.2).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_wz_read_back(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_WRITE_ZEROES)))
        return TEST_SKIP;
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 1) return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    uint16_t ai = 0;

    /* Seed sector 0 with a nonzero pattern. */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    memset(data, 0x5A, 512); *st = 0xFF;
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
    if (*st != VIRTIO_BLK_S_OK) TFAIL("seed write status %u", *st);

    /* Zero sector 0 via WRITE_ZEROES. */
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES; hdr->ioprio = 0; hdr->sector = 0;
    seg->sector = 0; seg->num_sectors = 1; seg->flags = 0;
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st == VIRTIO_BLK_S_UNSUPP) return TEST_SKIP;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write zeroes status %u", *st);

    /* Read sector 0 back and verify it is all zero. */
    hdr->type = VIRTIO_BLK_T_IN; hdr->ioprio = 0; hdr->sector = 0;
    memset(data, 0xFF, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);
    for (int i = 0; i < 512; i++)
        if (data[i] != 0)
            TFAIL("byte %d: 0x%02x != 0x00", i, data[i]);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0228, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_read_back,
              "Write zeroes then read returns zeros",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_WRITE_ZEROES), 0);
