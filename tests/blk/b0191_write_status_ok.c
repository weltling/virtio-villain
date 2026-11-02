/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0191: write returns S_OK status for valid request.
 *
 * Submit a single sector WRITE with valid data and verify the
 * status byte is VIRTIO_BLK_S_OK (0). Fundamental positive path
 * test for write operations.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_status_ok(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    memset(data, 0x55, 512);
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("write status %u, expected S_OK (0)", *st);

    return TEST_PASS;
}

REGISTER_TEST(B0191, VIRTIO_PCI_DEVICE_BLK, test_blk_write_status_ok,
              "Write returns S_OK status for valid request",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
