/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0002: net_tx_no_header
 *
 * Submit a TX descriptor shorter than sizeof(struct virtio_net_hdr).
 * The device must reject or error rather than reading past the buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_no_header(struct virtio_dev *dev,
                                           struct vring *vr)
{
    /* A 4-byte buffer - too short for the 10-byte virtio_net_hdr */
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 4, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0002, VIRTIO_PCI_DEVICE_NET, test_net_tx_no_header,
              "TX descriptor shorter than virtio_net_hdr",
              VIRTIO_SPEC_V1_2, "5.1.6");
