/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0188: tx_max_chain
 *
 * A transmit request spans a full ring worth of descriptors, the
 * header followed by many small readable fragments. The device must
 * gather the whole chain without overrunning the ring.
 */
#include "lib/virtio_test.h"

#define CHAIN 16

static test_result_t test_net_tx_max_chain(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frag = vv_alloc_pages(1);
    memset(frag, 0x5a, 64);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    for (int i = 1; i < CHAIN; i++) {
        uint16_t last = i == CHAIN - 1;
        vv_desc(vr, i, frag + (i * 64 % 4096), 64, last ? 0 : VRING_DESC_F_NEXT,
                last ? 0 : i + 1);
    }
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0188, VIRTIO_PCI_DEVICE_NET, test_net_tx_max_chain,
                "Transmit request spanning a full ring of descriptors",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
