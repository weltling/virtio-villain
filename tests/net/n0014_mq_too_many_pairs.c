/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0014: net_mq_too_many_pairs
 *
 * Send VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET with a value exceeding
 * max_virtqueue_pairs. Spec 5.1.6.5.6.1: driver MUST NOT request
 * more queue pairs than max_virtqueue_pairs.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

#define VIRTIO_NET_CTRL_MQ              4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET 0

static test_result_t test_net_mq_too_many_pairs(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint16_t *pairs = (uint16_t *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    /* Request 0xFFFF pairs - way beyond any device limit */
    *pairs = 0xFFFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(uint16_t),
                       VRING_DESC_F_NEXT, VV_QUEUE_LAST);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0014, VIRTIO_PCI_DEVICE_NET, test_net_mq_too_many_pairs,
              "MQ VQ_PAIRS_SET exceeding max pairs",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST);
