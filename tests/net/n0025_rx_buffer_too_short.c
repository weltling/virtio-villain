/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0025: net_rx_buffer_too_short
 *
 * Post an RX buffer chain whose total writable length is less than
 * the virtio_net_hdr size. The device should not write partial headers
 * or crash when there is insufficient buffer space.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_buffer_too_short(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* Post a 4-byte writable buffer - smaller than virtio_net_hdr (10/12) */
    vv_desc(vr, 0, buf, 4, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0025, VIRTIO_PCI_DEVICE_NET, test_net_rx_buffer_too_short,
              "RX buffer chain shorter than virtio_net_hdr",
              VIRTIO_SPEC_V1_2, "5.1.6.4.2", 0);
