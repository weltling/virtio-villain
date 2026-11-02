/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0067: TX packet on queue index beyond active virtqueue_pairs
 *
 * Spec 5.1.6.5.6: "The driver MUST NOT queue packets on transmit
 * queues greater than virtqueue_pairs once it has placed the
 * VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET command."
 *
 * Without sending VQ_PAIRS_SET (default=1 pair), transmit on
 * queue pair 2's TX queue (index 5).
 */
#include "lib/virtio_test.h"

static test_result_t test_tx_beyond_pairs(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;

    /* Need at least 3 TX/RX pairs + ctrl = 7 queues */
    if (cfg->num_queues < 7)
        return TEST_SKIP;

    /* TX queue for pair 2 is index 5 (RX0=0,TX0=1,RX1=2,TX1=3,RX2=4,TX2=5) */
    uint16_t tx_q = 5;

    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, tx_q);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0x55, 46);

    vv_desc(&txvr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&txvr, 1, frame, 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, tx_q, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0067, VIRTIO_PCI_DEVICE_NET, test_tx_beyond_pairs,
              "TX on queue pair 2 without enabling multiqueue (default=1 pair)",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6",
              (1ULL << VIRTIO_NET_F_MQ), 0);
