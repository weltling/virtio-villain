/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0031: net_rx_one_byte_buffer
 *
 * Post a 1-byte device-writable buffer on the RX queue. This is far
 * too small even for the virtio_net_hdr. Tests that the device handles
 * undersized RX buffers without writing beyond the descriptor length.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_one_byte(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    *buf = 0xCC;

    /* Single 1-byte writable descriptor on RX queue */
    vv_desc(vr, 0, buf, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0031, VIRTIO_PCI_DEVICE_NET, test_net_rx_one_byte,
              "RX buffer of 1 byte (too small for virtio_net_hdr)",
              VIRTIO_SPEC_V1_2, "5.1.6.4", 0);
