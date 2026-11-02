/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0005: net_rx_writable_check
 *
 * Submit an RX buffer descriptor without the WRITE flag. The device
 * needs to write received frames into this buffer; without WRITE it
 * must reject rather than writing into a read-only descriptor.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_writable(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* RX buffer without WRITE flag - device cannot write into it */
    vv_desc(vr, 0, buf, 1526, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0005, VIRTIO_PCI_DEVICE_NET, test_net_rx_writable,
              "RX buffer descriptor without WRITE flag",
              VIRTIO_SPEC_V1_2, "5.1.6");
