/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0227: flush between two writes preserves both.
 *
 * Write pattern A to sector 0, flush, write pattern B to sector 1,
 * flush, then read both back and verify. Tests that flushes
 * interspersed with writes do not lose or reorder data.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t submit_write(struct virtio_dev *dev, struct vring *vr,
                                  uint16_t *ai, uint64_t sector, uint8_t pat)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = sector;
    memset(data, pat, 512); *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, (*ai)++, 0);
    vring_raw_set_avail_idx(vr, *ai);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    return (*st == VIRTIO_BLK_S_OK) ? TEST_PASS : TEST_FAIL;
}

static test_result_t submit_flush(struct virtio_dev *dev, struct vring *vr,
                                  uint16_t *ai)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_FLUSH; hdr->ioprio = 0; hdr->sector = 0;
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, (*ai)++, 0);
    vring_raw_set_avail_idx(vr, *ai);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    return (*st == VIRTIO_BLK_S_OK) ? TEST_PASS : TEST_FAIL;
}

static test_result_t test_blk_flush_between(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_FLUSH)))
        return TEST_SKIP;
    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    if (!bcfg || bcfg->capacity < 2) return TEST_SKIP;

    uint16_t ai = 0;
    test_result_t r;
    r = submit_write(dev, vr, &ai, 0, 0x33); if (r != TEST_PASS) return r;
    r = submit_flush(dev, vr, &ai); if (r != TEST_PASS) return r;
    r = submit_write(dev, vr, &ai, 1, 0x44); if (r != TEST_PASS) return r;
    r = submit_flush(dev, vr, &ai); if (r != TEST_PASS) return r;

    /* Read both back */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    for (int s = 0; s < 2; s++) {
        hdr->type = VIRTIO_BLK_T_IN; hdr->ioprio = 0; hdr->sector = s;
        memset(data, 0, 512); *st = 0xFF;
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
        if (*st != VIRTIO_BLK_S_OK) TFAIL("read%d status %u", s, *st);
        uint8_t exp = s ? 0x44 : 0x33;
        for (int i = 0; i < 512; i++)
            if (data[i] != exp)
                TFAIL("sector%d byte %d: 0x%02x != 0x%02x", s, i, data[i], exp);
    }
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0227, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_between,
              "Flush between writes preserves both sectors",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_FLUSH), 0);
