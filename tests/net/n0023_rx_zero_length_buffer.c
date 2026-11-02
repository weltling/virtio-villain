/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0023: net_rx_zero_length_buffer
 *
 * Place a zero-length device-writable buffer in the receive queue.
 * The device needs space to write received packets; a zero-length
 * buffer may cause an underflow in size calculations or an attempt
 * to write to zero bytes of memory.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_zero_length(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* Place a zero-length writable descriptor in the queue */
    vv_desc(vr, 0, buf, 0, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0023, VIRTIO_PCI_DEVICE_NET, test_net_rx_zero_length,
              "Zero-length device-writable buffer in receiveq",
              VIRTIO_SPEC_V1_2, "5.1.6.4.2", 0);
