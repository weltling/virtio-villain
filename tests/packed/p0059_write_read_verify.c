/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0059: packed vring write then read data integrity.
 *
 * Write a known pattern via packed queue, read it back, and verify
 * every byte matches. Exercises the packed virtqueue data path for
 * correctness rather than just completion signaling.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test(struct virtio_dev *dev, struct vring_packed *vr)
{
    /* WRITE sector 0 */
    struct virtio_blk_outhdr *wh = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);
    wh->type = VIRTIO_BLK_T_OUT;
    wh->ioprio = 0;
    wh->sector = 0;
    for (int i = 0; i < 512; i++)
        wdata[i] = (uint8_t)(i ^ 0x3C);
    *wst = 0xFF;

    uint16_t head = vr->next_avail;
    uint8_t wrap = vr->wrap_counter;
    vring_packed_set_desc(vr, head, vv_virt_to_phys(wh), sizeof(*wh),
                          head, VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(wdata),
                          512, head, VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(wst),
                          1, head, VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    test_result_t r = vv_kick_and_wait_packed(dev, vr, vr->queue,
                                              head, wrap, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (*wst != VIRTIO_BLK_S_OK)
        TFAIL("write status %u", *wst);

    /* READ sector 0 */
    struct virtio_blk_outhdr *rh = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);
    rh->type = VIRTIO_BLK_T_IN;
    rh->ioprio = 0;
    rh->sector = 0;
    memset(rdata, 0, 512);
    *rst = 0xFF;

    head = vr->next_avail;
    wrap = vr->wrap_counter;
    vring_packed_set_desc(vr, head, vv_virt_to_phys(rh), sizeof(*rh),
                          head, VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(rdata),
                          512, head,
                          VRING_PACKED_DESC_F_NEXT |
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(rst),
                          1, head, VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    r = vv_kick_and_wait_packed(dev, vr, vr->queue, head, wrap,
                                VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (*rst != VIRTIO_BLK_S_OK)
        TFAIL("read status %u", *rst);

    /* Verify data */
    for (int i = 0; i < 512; i++) {
        uint8_t expected = (uint8_t)(i ^ 0x3C);
        if (rdata[i] != expected)
            TFAIL("byte %d: got 0x%02x, want 0x%02x",
                  i, rdata[i], expected);
    }
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0059, VIRTIO_PCI_DEVICE_BLK, test,
                     "Packed write then read verifies data integrity",
                     VIRTIO_SPEC_V1_2, "2.8");
