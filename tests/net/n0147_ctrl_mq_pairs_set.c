/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0147: ctrl MQ set virtqueue pairs valid.
 *
 * Spec 5.1.6.5.5: When VIRTIO_NET_F_MQ is negotiated the driver
 * sets the number of active queue pairs via CTRL_MQ_VQ_PAIRS_SET.
 * Set pairs=1 (minimum valid) and verify VIRTIO_NET_OK.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_mq_set(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CTRL_VQ)))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint16_t *pairs = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MQ;
    hdr->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    *pairs = 1;  /* minimum valid */
    *ack = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, pairs, 2, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    if (*ack != VIRTIO_NET_OK)
        TFAIL("ack %u, expected VIRTIO_NET_OK", *ack);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0147, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mq_set,
              "CTRL MQ set pairs to 1",
              VIRTIO_SPEC_V1_2, "5.1.6.5.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_MQ) |
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
