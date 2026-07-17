/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0226: gathered write from three descriptors verify.
 *
 * Write 512 bytes assembled from three readable descriptors of
 * 100, 300, and 112 bytes, then read the sector back in one
 * descriptor and verify the concatenated data. Tests the device
 * read side gather across a descriptor chain.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_gather_write(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *w0 = vv_alloc_pages(1);
    uint8_t *w1 = vv_alloc_pages(1);
    uint8_t *w2 = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    for (int i = 0; i < 100; i++) w0[i] = (uint8_t)(i ^ 0x71);
    for (int i = 0; i < 300; i++) w1[i] = (uint8_t)((i + 100) ^ 0x71);
    for (int i = 0; i < 112; i++) w2[i] = (uint8_t)((i + 400) ^ 0x71);
    *st = 0xFF;

    /* WRITE gathered from three data descriptors */
    hdr->type = VIRTIO_BLK_T_OUT; hdr->ioprio = 0; hdr->sector = 0;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(w0), 100,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(w1), 300,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(w2), 112,
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("write status %u", *st);

    /* READ back in one descriptor */
    uint8_t *rd = vv_alloc_pages(1);
    memset(rd, 0, 512); *st = 0xFF;
    hdr->type = VIRTIO_BLK_T_IN;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(rd), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("read status %u", *st);

    for (int i = 0; i < 512; i++)
        if (rd[i] != (uint8_t)(i ^ 0x71))
            TFAIL("byte %d: 0x%02x != 0x%02x", i, rd[i], (uint8_t)(i ^ 0x71));

    return TEST_PASS;
}

REGISTER_TEST(B0226, VIRTIO_PCI_DEVICE_BLK, test_blk_gather_write,
              "Gathered write from three descriptors verify",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
