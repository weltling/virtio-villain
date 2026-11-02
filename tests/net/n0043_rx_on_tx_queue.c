/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0043: net_rx_buffer_on_tx_queue
 *
 * Post a writable (RX-style) buffer on the TX queue instead of the
 * RX queue. Tests whether the device validates queue directionality
 * or blindly processes writable descriptors as received frames.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_on_tx_queue(struct virtio_dev *dev,
                                             struct vring *vr)
{
    /* vr is the TX queue (queue 1). Post a writable buffer on it. */
    uint8_t *buf = vv_alloc_pages(1);

    /* Mark as device-writable (like an RX buffer) on the TX queue */
    vv_desc(vr, 0, buf, 4096, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0043, VIRTIO_PCI_DEVICE_NET, test_net_rx_on_tx_queue,
              "Writable (RX-style) buffer posted on TX queue",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
