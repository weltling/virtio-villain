/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0013: desc_no_status
 *
 * Submit a request with a header and data descriptor but no status
 * descriptor. The spec requires a trailing 1-byte writable descriptor
 * for status. A VMM that blindly expects the third descriptor may
 * read past the chain or crash.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_no_status(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);

    /*
     * Descriptor chain: header -> data, NO status descriptor.
     * The chain ends at [1] (no NEXT flag).
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_WRITE, 0); /* no NEXT -> chain ends */

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0013, VIRTIO_PCI_DEVICE_BLK, test_desc_no_status,
              "Chain with no status descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
