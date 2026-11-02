/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0014: desc_only_header
 *
 * Submit a request with only the header descriptor - no data, no status.
 * This is an even more truncated chain than T13.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_only_header(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);

    /*
     * Single descriptor: just the header, no NEXT flag.
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0014, VIRTIO_PCI_DEVICE_BLK, test_desc_only_header,
              "Chain with only header descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
