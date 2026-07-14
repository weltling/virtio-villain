/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0170: write_zeroes_then_read
 *
 * Submit a WRITE_ZEROES over sectors 0..7, wait for completion,
 * then READ sector 0. Spec 5.2.6.2 says "After a write zeroes
 * command is completed, reads of the specified ranges of sectors
 * MUST return zeroes." Verify the read data is all zeros.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_wz_then_read(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_WRITE_ZEROES)))
        return TEST_SKIP;

    /* First: write non-zero data to sector 0 so we know zeroes are fresh */
    struct virtio_blk_outhdr *whdr = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);

    whdr->type = VIRTIO_BLK_T_OUT;
    whdr->ioprio = 0;
    whdr->sector = 0;
    memset(wdata, 0xAA, 512);
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
    if (r != TEST_PASS)
        return r;
    if (*wst != VIRTIO_BLK_S_OK)
        TFAIL("write returned status %u", *wst);

    /* Second: WRITE_ZEROES sectors 0..7 */
    struct virtio_blk_outhdr *zhdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *zseg = vv_alloc_pages(1);
    uint8_t *zst = vv_alloc_pages(1);

    zhdr->type = VIRTIO_BLK_T_WRITE_ZEROES;
    zhdr->ioprio = 0;
    zhdr->sector = 0;
    zseg->sector = 0;
    zseg->num_sectors = 8;
    zseg->flags = 0;
    *zst = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(zhdr), sizeof(*zhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(zseg), sizeof(*zseg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(zst), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (*zst != VIRTIO_BLK_S_OK)
        TFAIL("write_zeroes returned status %u", *zst);

    /* Third: READ sector 0, verify all zeros */
    struct virtio_blk_outhdr *rhdr = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    rhdr->type = VIRTIO_BLK_T_IN;
    rhdr->ioprio = 0;
    rhdr->sector = 0;
    memset(rdata, 0xBB, 512);
    *rst = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rhdr), sizeof(*rhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(rdata), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(rst), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 2, 0);
    vring_raw_set_avail_idx(vr, 3);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (*rst != VIRTIO_BLK_S_OK)
        TFAIL("read returned status %u", *rst);

    /* Verify data is all zeros */
    for (int i = 0; i < 512; i++) {
        if (rdata[i] != 0)
            TFAIL("byte %d is 0x%02x, expected 0x00", i, rdata[i]);
    }
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0170, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_then_read,
              "Write zeroes then read verifies data is all zeros",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_WRITE_ZEROES), 0);
