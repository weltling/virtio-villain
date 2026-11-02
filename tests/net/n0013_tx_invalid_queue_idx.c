/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0013: net_tx_invalid_queue_idx
 *
 * Queue a packet on a TX queue index greater than the negotiated
 * number of queue pairs minus one. Spec 5.1.6.5.6.1: driver MUST NOT
 * queue packets on transmit queues greater than virtqueue_pairs.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_invalid_queue_idx(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    memset(frame, 0x42, 64);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, frame, 64, 0, 0);

    vring_submit(vr, 0);

    /*
     * Kick queue index 99 - far beyond what any net device with
     * default single queue pair would have configured.
     */
    return vv_kick_and_wait(dev, vr, 99, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0013, VIRTIO_PCI_DEVICE_NET, test_net_tx_invalid_queue_idx,
              "Kick invalid queue index beyond pairs",
              VIRTIO_SPEC_V1_2, "5.1.6.5");
