/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0179: write data split across two descriptors.
 *
 * Spec 2.7.5: Data may be split across multiple descriptors chained
 * via NEXT. Write 1024 bytes to sector 0 using two 512 byte data
 * descriptors, then read the full 1024 bytes back in one descriptor
 * and verify every byte matches.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_write_scattered(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *whdr = vv_alloc_pages(1);
    uint8_t *d1 = vv_alloc_pages(1);
    uint8_t *d2 = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);

    whdr->type = VIRTIO_BLK_T_OUT;
    whdr->ioprio = 0;
    whdr->sector = 0;
    for (int i = 0; i < 512; i++) d1[i] = (uint8_t)(i ^ 0xDE);
    for (int i = 0; i < 512; i++) d2[i] = (uint8_t)(i ^ 0xAD);
    *wst = 0xFF;

    /* hdr -> data1(512) -> data2(512) -> status */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(whdr), sizeof(*whdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(d1), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(d2), 512,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(wst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*wst != VIRTIO_BLK_S_OK) TFAIL("write status %u", *wst);

    /* Read 1024 bytes back in one descriptor */
    struct virtio_blk_outhdr *rhdr = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    rhdr->type = VIRTIO_BLK_T_IN;
    rhdr->ioprio = 0;
    rhdr->sector = 0;
    memset(rdata, 0, 1024);
    *rst = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rhdr), sizeof(*rhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(rdata), 1024,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(rst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*rst != VIRTIO_BLK_S_OK) TFAIL("read status %u", *rst);

    for (int i = 0; i < 512; i++) {
        uint8_t e = (uint8_t)(i ^ 0xDE);
        if (rdata[i] != e)
            TFAIL("byte %d: 0x%02x != 0x%02x", i, rdata[i], e);
    }
    for (int i = 0; i < 512; i++) {
        uint8_t e = (uint8_t)(i ^ 0xAD);
        if (rdata[512 + i] != e)
            TFAIL("byte %d: 0x%02x != 0x%02x", 512 + i, rdata[512 + i], e);
    }
    return TEST_PASS;
}

REGISTER_TEST(B0179, VIRTIO_PCI_DEVICE_BLK, test_blk_write_scattered,
              "Write data split across two descriptors then verify",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
