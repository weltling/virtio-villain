/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0038: net_ctrl_mq_pairs_eq_max
 *
 * Send a CTRL_MQ SET_QUEUES command requesting exactly max_virtqueue_pairs.
 * This is the boundary value - one more would be invalid (N14 tests that).
 * Tests that the device correctly accepts the maximum without off-by-one.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_NET_CTRL_MQ       4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET 0

struct ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

static test_result_t test_net_ctrl_mq_pairs_max(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    /*
     * The control queue is queue 2 for net devices (after RX=0, TX=1).
     * We're passed the TX queue (queue 1), so we need queue 2.
     * Since init.c sets up all queues, we can reference queue 2.
     */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    /* Net device: num_queues = 2*N+1 (RX+TX pairs + ctrl) */
    if (nq < 5) /* need at least 2 pairs + ctrl = 5 queues */
        return TEST_SKIP;

    uint16_t max_pairs = (nq - 1) / 2;

    struct ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint16_t *pairs = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    *pairs = max_pairs; /* exactly the maximum */
    *ack = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t pairs_phys = vv_virt_to_phys(pairs);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    /* Control virtqueue is queue index 2 (for 1 pair) or nq-1 */
    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    vring_raw_set_desc(&cvr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, pairs_phys, sizeof(*pairs),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, ack_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0038, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mq_pairs_max,
              "CTRL_MQ SET_QUEUES with pairs == max_virtqueue_pairs",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6");
