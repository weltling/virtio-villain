/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0196: GET_ID response contains printable ASCII.
 *
 * Spec 5.2.6.2: GET_ID returns an ASCII string up to 20 bytes.
 * The driver clears the buffer because a short identifier need not
 * write the remaining bytes. Verify the resulting fixed buffer contains
 * only printable ASCII before the first NUL.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_format(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *id = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    memset(id, 0, VIRTIO_BLK_ID_BYTES);
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, id, VIRTIO_BLK_ID_BYTES,
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("status %u", *st);

    /* The device may write fewer bytes for a short identifier. */
    int past_nul = 0;
    for (int i = 0; i < VIRTIO_BLK_ID_BYTES; i++) {
        if (id[i] == 0) { past_nul = 1; continue; }
        if (past_nul)
            TFAIL("non-NUL byte 0x%02x at offset %d after NUL", id[i], i);
        if (id[i] < 0x20 || id[i] > 0x7E)
            TFAIL("non-printable byte 0x%02x at offset %d", id[i], i);
    }

    return TEST_PASS;
}

REGISTER_TEST(B0196, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_format,
              "GET_ID response contains printable ASCII",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
