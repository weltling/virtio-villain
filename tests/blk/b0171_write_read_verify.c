/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0171: write_read_verify
 *
 * Write a known pattern to sector 0, then read it back and verify
 * every byte matches. This is the basic data integrity roundtrip
 * that spec 5.2.6.2 implies: the device must faithfully store and
 * return submitted data.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_write_read_verify(struct virtio_dev *dev,
                                                struct vring *vr)
{
    /* Write a known pattern */
    struct virtio_blk_outhdr *whdr = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);

    whdr->type = VIRTIO_BLK_T_OUT;
    whdr->ioprio = 0;
    whdr->sector = 0;
    for (int i = 0; i < 512; i++)
        wdata[i] = (uint8_t)(i ^ 0xA5);
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
        TFAIL("write status %u", *wst);

    /* Read it back */
    struct virtio_blk_outhdr *rhdr = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    rhdr->type = VIRTIO_BLK_T_IN;
    rhdr->ioprio = 0;
    rhdr->sector = 0;
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
    if (r != TEST_PASS)
        return r;
    if (*rst != VIRTIO_BLK_S_OK)
        TFAIL("read status %u", *rst);

    /* Verify */
    for (int i = 0; i < 512; i++) {
        uint8_t expected = (uint8_t)(i ^ 0xA5);
        if (rdata[i] != expected)
            TFAIL("byte %d: got 0x%02x, want 0x%02x",
                  i, rdata[i], expected);
    }
    return TEST_PASS;
}

REGISTER_TEST(B0171, VIRTIO_PCI_DEVICE_BLK, test_blk_write_read_verify,
              "Write then read back verifies data integrity",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
