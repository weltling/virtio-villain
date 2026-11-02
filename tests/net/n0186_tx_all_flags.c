/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0186: tx_all_flags
 *
 * A transmit descriptor sets NEXT, WRITE and INDIRECT together, a
 * contradictory combination. The device must handle the conflicting
 * flags gracefully rather than fault the host.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_all_flags(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);

    vv_desc(vr, 0, hdr, sizeof(*hdr),
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE | VRING_DESC_F_INDIRECT, 1);
    vv_desc(vr, 1, hdr, sizeof(*hdr), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0186, VIRTIO_PCI_DEVICE_NET, test_net_tx_all_flags,
                "Transmit descriptor with contradictory flags",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
