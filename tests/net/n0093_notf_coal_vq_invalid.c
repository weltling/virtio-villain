/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0093: net_notf_coal_vq_invalid_idx
 *
 * Send VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET with a vq_index that
 * exceeds the number of virtqueues. The device should reject it.
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

#define VIRTIO_NET_CTRL_NOTF_COAL        6
#define VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET 2

struct virtio_net_ctrl_coal_vq {
    uint16_t vq_index;
    uint16_t reserved;
    uint32_t max_packets;
    uint32_t max_usecs;
} __attribute__((packed));

static test_result_t test_net_notf_coal_vq_bad(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_coal_vq *cvq =
        (struct virtio_net_ctrl_coal_vq *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_NOTF_COAL;
    ctrl->command = VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET;
    cvq->vq_index = 0xFFFF;  /* Invalid VQ index */
    cvq->reserved = 0;
    cvq->max_packets = 16;
    cvq->max_usecs = 250;
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(*cvq),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0093, VIRTIO_PCI_DEVICE_NET, test_net_notf_coal_vq_bad,
              "NOTF_COAL VQ_SET with invalid vq_index",
              VIRTIO_SPEC_V1_3, "5.1.6.5", VV_QUEUE_LAST);
