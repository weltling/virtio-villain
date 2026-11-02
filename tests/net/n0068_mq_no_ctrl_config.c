/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0068: MQ use queues without prior CTRL_MQ configuration
 *
 * Spec 5.1.6.5.6: "The driver MUST configure the virtqueues before
 * enabling them with the VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET command."
 * Also: "The driver MUST queue packets only on any transmitq1
 * before the VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET command."
 *
 * Submit a TX packet on pair 1 (TX queue index 3) without ever
 * sending VQ_PAIRS_SET. The queue is attached/enabled but
 * multiqueue is never activated via the control path.
 */
#include "lib/virtio_test.h"

static test_result_t test_mq_no_ctrl_config(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;

    /* Need at least 2 TX/RX pairs + ctrl = 5 queues */
    if (cfg->num_queues < 5)
        return TEST_SKIP;

    /* TX queue for pair 1 is index 3 */
    uint16_t tx_q = 3;

    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, tx_q);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0xAA, 46);

    vv_desc(&txvr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&txvr, 1, frame, 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, tx_q, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV_REQUIRES(N0068, VIRTIO_PCI_DEVICE_NET, test_mq_no_ctrl_config,
                  "TX on pair 1 without CTRL_MQ VQ_PAIRS_SET (unconfigured MQ)",
                  VIRTIO_SPEC_V1_2, "5.1.6.5.6",
                  (1ULL << VIRTIO_NET_F_MQ), 0);
