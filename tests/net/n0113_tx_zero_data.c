/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0113: Net TX with zero length data buffer.
 *
 * Spec 5.1.6: Submit a transmit request where the virtio_net_hdr
 * is followed by a zero length data descriptor. The device must
 * not crash on an empty frame body.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_zero_data(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr_mrg *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    /* Header descriptor (readable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* Data descriptor with zero length */
    vv_desc(vr, 1, data, 0, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0113, VIRTIO_PCI_DEVICE_NET, test_net_tx_zero_data,
                "Net TX with zero length data buffer",
                VIRTIO_SPEC_V1_2, "5.1.6", 1);
