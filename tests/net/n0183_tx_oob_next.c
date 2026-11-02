/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0183: tx_oob_next
 *
 * A transmit descriptor sets VRING_DESC_F_NEXT with its next field
 * beyond the queue size. The device must refuse the out of range link
 * without harm to the host.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_oob_next(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 0xFFFF);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0183, VIRTIO_PCI_DEVICE_NET, test_net_tx_oob_next,
                "Transmit descriptor next index out of bounds",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
