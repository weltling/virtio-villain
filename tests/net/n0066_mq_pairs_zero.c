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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define VIRTIO_NET_CTRL_MQ              4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET 0
#define VIRTIO_NET_F_MQ                 22

struct ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

struct virtio_net_ctrl_mq {
    uint16_t virtqueue_pairs;
} __attribute__((packed));

static test_result_t test_mq_pairs_zero(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 5)
        return TEST_SKIP;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_NET_F_MQ)))
        return TEST_SKIP;

    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_mq *mq_data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    mq_data->virtqueue_pairs = 0; /* violates MUST NOT */
    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(mq_data), sizeof(*mq_data),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0066, VIRTIO_PCI_DEVICE_NET, test_mq_pairs_zero,
              "CTRL_MQ VQ_PAIRS_SET with virtqueue_pairs=0 (violates min)",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6");
