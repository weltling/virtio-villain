/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0215: GET_ID returns S_OK status.
 *
 * Submit GET_ID with proper 20 byte writable buffer and verify
 * the status byte is S_OK. Explicitly checks the status field
 * rather than just used ring advancement.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_ok(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *id = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_GET_ID; hdr->ioprio = 0; hdr->sector = 0;
    memset(id, 0, 20); *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, id, 20, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("status %u", *st);

    return TEST_PASS;
}

REGISTER_TEST(B0215, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_ok,
              "GET_ID returns S_OK status",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
