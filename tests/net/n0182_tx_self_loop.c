/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0182: tx_self_loop
 *
 * A transmit descriptor sets VRING_DESC_F_NEXT with its next field
 * pointing back at itself, forming an endless chain. The device must
 * bound its walk of the descriptor chain rather than loop forever.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_self_loop(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);

    /* next points at descriptor 0, so the chain never terminates. */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0182, VIRTIO_PCI_DEVICE_NET, test_net_tx_self_loop,
                "Transmit descriptor chained to itself",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
