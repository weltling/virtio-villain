/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0108: net_mq_config_during_queue_reset
 *
 * Change MQ queue pairs via CTRL_MQ while a queue is being reset.
 * Spec v1.3 5.1.6.5.5 + 2.6.1: the device must handle MQ config
 * changes while individual queues are in reset state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_mq_during_reset(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset queue 2 (second RX queue if MQ active) */
    cfg->queue_select = 2;
    __sync_synchronize();
    uint16_t qs = cfg->queue_size;
    if (qs == 0)
        return TEST_SKIP;  /* Only 1 queue pair, cannot test MQ */

    /* Reset queue 2 via the queue_reset register (spec 2.6.1) */
    if (virtio_pci_queue_reset(dev, 2) < 0)
        return TEST_SKIP;

    /* Now send MQ VQ_PAIRS_SET via ctrl queue */
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_mq_pairs *pairs =
        (struct virtio_net_ctrl_mq_pairs *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    pairs->virtqueue_pairs = 1;  /* Reduce to 1 pair */
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(*pairs),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0108, VIRTIO_PCI_DEVICE_NET, test_net_mq_during_reset,
                "MQ VQ_PAIRS_SET while queue is in reset",
                VIRTIO_SPEC_V1_3, "5.1.6.5.5", VV_QUEUE_LAST);
