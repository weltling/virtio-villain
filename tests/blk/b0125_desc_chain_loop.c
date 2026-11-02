/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0125: Descriptor chain loop in block request.
 *
 * Create a request where the data descriptor's next index points back
 * to the header, forming a loop. The device must detect the cycle.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_desc_chain_loop(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);

    /* desc 0 -> desc 1 -> desc 0 (loop) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0125, VIRTIO_PCI_DEVICE_BLK, test_blk_desc_chain_loop,
              "Descriptor chain loop in request",
              VIRTIO_SPEC_V1_2, "5.2.6");
