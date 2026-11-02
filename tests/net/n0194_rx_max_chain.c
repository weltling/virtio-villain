/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0194: rx_max_chain
 *
 * A receive buffer spans a full ring of writable descriptors. The
 * device must accept the long chain without overrunning the ring.
 */
#include "lib/virtio_test.h"

#define CHAIN 16

static test_result_t test_net_rx_max_chain(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    for (int i = 0; i < CHAIN; i++) {
        uint16_t last = i == CHAIN - 1;
        vv_desc(vr, i, buf + (i * 128 % 4096), 128,
                VRING_DESC_F_WRITE | (last ? 0 : VRING_DESC_F_NEXT),
                last ? 0 : i + 1);
    }
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0194, VIRTIO_PCI_DEVICE_NET, test_net_rx_max_chain,
                "Receive buffer spanning a full ring of descriptors",
                VIRTIO_SPEC_V1_2, "2.7.5", 0);
