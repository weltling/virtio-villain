/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0001: desc_chain_loop
 *
 * Create a descriptor chain that loops back to itself. The descriptor's
 * next field points to its own index, forming a cycle. A VMM that walks
 * the chain without tracking visited indices will loop forever (hang or
 * 100% CPU in the virtio worker thread).
 */
#include "lib/virtio_test.h"

/* virtio-blk request header */
static test_result_t test_desc_chain_loop(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);

    /*
     * Descriptor chain: [0] header -> [1] data -> [1] (loop!)
     * Descriptor 1's next points back to itself.
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 1);

    /* Submit head 0 */
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0001, VIRTIO_PCI_DEVICE_BLK, test_desc_chain_loop,
              "Descriptor chain loop back to self",
              VIRTIO_SPEC_V1_2, "2.7.5");
