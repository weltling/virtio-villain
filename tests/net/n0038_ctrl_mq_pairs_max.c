/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0038: net_ctrl_mq_pairs_eq_max
 *
 * Send a CTRL_MQ SET_QUEUES command requesting exactly max_virtqueue_pairs.
 * This is the boundary value - one more would be invalid (N14 tests that).
 * Tests that the device correctly accepts the maximum without off-by-one.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_mq_pairs_max(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    uint16_t max_pairs = (nq - 1) / 2;
    if (max_pairs < 1)
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint16_t *pairs = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    *pairs = max_pairs; /* exactly the maximum */
    *ack = 0xFF;

    vv_desc(vr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, pairs, sizeof(*pairs), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0038, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mq_pairs_max,
              "CTRL_MQ SET_QUEUES with pairs == max_virtqueue_pairs",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_MQ), 0);
