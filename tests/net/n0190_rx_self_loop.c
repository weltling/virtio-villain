/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0190: rx_self_loop
 *
 * A receive descriptor sets VRING_DESC_F_NEXT with its next field
 * pointing back at itself. The device must bound its walk of the
 * chain rather than loop forever.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_self_loop(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 1526, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0190, VIRTIO_PCI_DEVICE_NET, test_net_rx_self_loop,
                "Receive descriptor chained to itself",
                VIRTIO_SPEC_V1_2, "2.7.5", 0);
