/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0026: net_ctrl_mq_pairs_zero
 *
 * Issue VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET with virtqueue_pairs=0.
 * Spec 5.1.6.5.6.1: the device expects a valid number of queue pairs
 * (1 <= pairs <= max). Zero is invalid and must not crash the device.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_mq_zero(struct virtio_dev *dev,
                                           struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint16_t *pairs = (uint16_t *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    *pairs = 0; /* invalid: zero pairs */
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);

    /* descriptor 0: header (device-readable) */
    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    /* descriptor 1: pairs value (device-readable) */
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), 2,
                       VRING_DESC_F_NEXT, 2);
    /* descriptor 2: status byte (device-writable) */
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0026, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mq_zero,
              "CTRL_MQ VQ_PAIRS_SET with pairs=0",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_MQ), 0);
