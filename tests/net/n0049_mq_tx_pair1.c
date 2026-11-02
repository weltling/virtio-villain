/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0049: net_multiqueue_tx_nonzero_pair
 *
 * Send a TX packet on queue pair > 0 (queue index 2 for TX, since
 * pairs are RX0/TX0=0/1, RX1/TX1=2/3, etc.). Tests device
 * multiqueue transmit path beyond the default pair.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_mq_tx_pair1(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Need at least 4 queues for pair 1 (queues 0,1,2,3) + ctrl */
    if (cfg->num_queues < 5)
        return TEST_SKIP;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;

    /* TX queue for pair 1 is queue index 3 (RX1=2, TX1=3) */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 3);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    /* Simple ethernet frame */
    memset(frame, 0xFF, 6);       /* dst broadcast */
    memset(frame + 6, 0x02, 6);   /* src */
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0x55, 46); /* minimum payload */

    vv_desc(&txvr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&txvr, 1, frame, 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV_REQUIRES(N0049, VIRTIO_PCI_DEVICE_NET, test_net_mq_tx_pair1,
                  "Multiqueue TX on queue pair 1 (non-default pair)",
                  VIRTIO_SPEC_V1_2, "5.1.6.5",
                  (1ULL << VIRTIO_NET_F_MQ), 0);
