/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0066: CTRL_MQ VQ_PAIRS_SET with virtqueue_pairs = 0
 *
 * Spec 5.1.6.5.6: "The driver MUST NOT request a virtqueue_pairs
 * of 0 or greater than max_virtqueue_pairs in the device
 * configuration space."
 *
 * Send VQ_PAIRS_SET with pairs = 0.
 */
#include "lib/virtio_test.h"

static test_result_t test_mq_pairs_zero(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 5)
        return TEST_SKIP;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;

    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_mq *mq_data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    mq_data->virtqueue_pairs = 0; /* violates MUST NOT */
    *ack = 0xFF;

    vv_desc(&cvr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vv_desc(&cvr, 1, mq_data, sizeof(*mq_data), VRING_DESC_F_NEXT, 2);
    vv_desc(&cvr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV_REQUIRES(N0066, VIRTIO_PCI_DEVICE_NET, test_mq_pairs_zero,
                  "CTRL_MQ VQ_PAIRS_SET with virtqueue_pairs=0 (violates min)",
                  VIRTIO_SPEC_V1_2, "5.1.6.5.6",
                  (1ULL << VIRTIO_NET_F_MQ), 0);
