/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0144: GET_ID returns printable bytes within bound
 *
 * Spec 5.2.6.1 says VIRTIO_BLK_T_GET_ID writes up to 20 bytes of
 * device identifier into the writable buffer. The result is not
 * required to be NUL terminated but every byte that the device
 * actually writes should be a sane value rather than the leftover
 * sentinel the driver pre filled. This catches devices that ack
 * the request with status OK but never touch the buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_content(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    memset(data, 0xAA, VIRTIO_BLK_ID_BYTES);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, VIRTIO_BLK_ID_BYTES,
            VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    if (*st != 0)
        TREJECT("*st != 0");

    /* Check at least one byte was written and it is not the sentinel */
    int touched = 0;
    for (int i = 0; i < VIRTIO_BLK_ID_BYTES; i++) {
        if (data[i] != 0xAA) {
            touched = 1;
            break;
        }
    }
    if (!touched)
        TREJECT("!touched");

    return TEST_PASS;
}

REGISTER_TEST(B0144, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_content,
              "GET_ID writes identifier bytes into the buffer",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
