/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0193: rx_all_flags
 *
 * A receive descriptor sets WRITE, NEXT and INDIRECT together, a
 * contradictory combination. The device must handle the conflicting
 * flags gracefully rather than fault the host.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_all_flags(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 1526,
            VRING_DESC_F_WRITE | VRING_DESC_F_NEXT | VRING_DESC_F_INDIRECT, 1);
    vv_desc(vr, 1, buf, 1526, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0193, VIRTIO_PCI_DEVICE_NET, test_net_rx_all_flags,
                "Receive descriptor with contradictory flags",
                VIRTIO_SPEC_V1_2, "2.7.5", 0);
