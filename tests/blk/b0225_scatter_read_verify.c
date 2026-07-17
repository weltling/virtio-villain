/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0225: scatter read into three descriptors verify.
 *
 * Write a known 512 byte pattern to sector 0, then read it back
 * into three writable descriptors of 128, 256, and 128 bytes.
 * Verify the reassembled data matches. Tests device write side
 * scatter across a descriptor chain.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_scatter_read(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    /* Write pattern */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    for (int i = 0; i < 512; i++) wdata[i] = (uint8_t)(i ^ 0x93);
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(wdata), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write status %u", *st);

    /* Read into three segments: 128 + 256 + 128 */
    uint8_t *r0 = vv_alloc_pages(1);
    uint8_t *r1 = vv_alloc_pages(1);
    uint8_t *r2 = vv_alloc_pages(1);
    memset(r0, 0, 128); memset(r1, 0, 256); memset(r2, 0, 128);
    *st = 0xFF;

    hdr->type = VIRTIO_BLK_T_IN;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(r0), 128,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(r1), 256,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(r2), 128,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    /* Verify reassembled data */
    for (int i = 0; i < 128; i++)
        if (r0[i] != (uint8_t)(i ^ 0x93))
            TFAIL("seg0 byte %d mismatch", i);
    for (int i = 0; i < 256; i++)
        if (r1[i] != (uint8_t)((i + 128) ^ 0x93))
            TFAIL("seg1 byte %d mismatch", i);
    for (int i = 0; i < 128; i++)
        if (r2[i] != (uint8_t)((i + 384) ^ 0x93))
            TFAIL("seg2 byte %d mismatch", i);

    return TEST_PASS;
}

REGISTER_TEST(B0225, VIRTIO_PCI_DEVICE_BLK, test_blk_scatter_read,
              "Scatter read into three descriptors verify",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
