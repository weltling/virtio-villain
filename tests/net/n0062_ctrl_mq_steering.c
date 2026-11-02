/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0062: CTRL_MQ VQ_PAIRS_SET channel steering (spec 5.1.6.5.5)
 *
 * Set the active number of queue pairs via CTRL_MQ. Attempt to
 * set it to a value beyond the device maximum to test validation.
 */
#include "lib/virtio_test.h"

static test_result_t test_ctrl_mq_steering(struct virtio_dev *dev,
                                           struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_mq *mq_data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    /* Set to 0xFFFF — way beyond any device maximum */
    mq_data->virtqueue_pairs = 0xFFFF;
    *ack = 0xFF;

    vv_desc(vr, 0, ctrl, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, mq_data, sizeof(*mq_data), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0062, VIRTIO_PCI_DEVICE_NET, test_ctrl_mq_steering,
              "CTRL_MQ VQ_PAIRS_SET with 0xFFFF pairs (overflow)",
              VIRTIO_SPEC_V1_2, "5.1.6.5.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_MQ), 0);
