/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0077: net_ctrl_mq_pairs
 *
 * Send VIRTIO_NET_CTRL_MQ VQ_PAIRS_SET to configure multi-queue.
 * Spec 5.1.6.5.5: The driver uses this command to set the number
 * of active queue pairs for multiqueue operation.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_mq_pairs(struct virtio_dev *dev,
                                            struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_MQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    struct virtio_net_ctrl_mq *mq = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MQ;
    hdr->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    mq->virtqueue_pairs = 1;  /* Set to 1 pair (minimum valid) */
    *ack = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t mq_phys = vv_virt_to_phys(mq);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, mq_phys, sizeof(*mq),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, ack_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*ack != VIRTIO_NET_OK)
        TFAIL("ack %u, expected VIRTIO_NET_OK (0)", *ack);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0077, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mq_pairs,
              "Control VQ: set multiqueue pairs",
              VIRTIO_SPEC_V1_2, "5.1.6.5.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_MQ), 0);
