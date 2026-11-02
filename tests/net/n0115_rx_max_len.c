/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0115: Net RX with maximum length descriptor.
 *
 * Spec 5.1.6: Post a receive buffer with len = 0xFFFFFFFF.
 * The device must not overflow internal calculations when
 * confronted with a maximum size writable descriptor.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_max_len(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 0xFFFFFFFFU, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(N0115, VIRTIO_PCI_DEVICE_NET, test_net_rx_max_len,
              "Net RX buffer with maximum descriptor length",
              VIRTIO_SPEC_V1_2, "5.1.6");
