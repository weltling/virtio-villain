/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0222: write, flush, then read verifies data persisted.
 *
 * Write a pattern to sector 0, flush to ensure stability, then
 * read back and verify the data. This is the canonical write
 * persistence test: spec 5.2.6.2 says after flush completes all
 * prior writes are stable.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_wfr(struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_FLUSH)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    uint16_t ai = 0;

    /* WRITE */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    for (int i = 0; i < 512; i++) data[i] = (uint8_t)(i ^ 0xBC);
    *st = 0xFF;
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
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write status %u", *st);

    /* FLUSH */
    hdr->type = VIRTIO_BLK_T_FLUSH; hdr->sector = 0;
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("flush status %u", *st);

    /* READ and verify */
    hdr->type = VIRTIO_BLK_T_IN; hdr->sector = 0;
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
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    for (int i = 0; i < 512; i++) {
        uint8_t e = (uint8_t)(i ^ 0xBC);
        if (data[i] != e)
            TFAIL("byte %d: 0x%02x != 0x%02x", i, data[i], e);
    }
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0222, VIRTIO_PCI_DEVICE_BLK, test_blk_wfr,
              "Write, flush, read verifies data persisted",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_FLUSH), 0);
