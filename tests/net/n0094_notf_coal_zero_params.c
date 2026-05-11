/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0094: net_notf_coal_zero_params
 *
 * Send NOTF_COAL TX_SET with max_packets=0 and max_usecs=0.
 * Spec says these are best effort; zero values should disable
 * coalescing or be accepted gracefully.
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
#define VIRTIO_NET_CTRL_NOTF_COAL_TX_SET 0

struct virtio_net_ctrl_coal {
    uint32_t max_packets;
    uint32_t max_usecs;
} __attribute__((packed));

static test_result_t test_net_notf_coal_zero(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_coal *coal =
        (struct virtio_net_ctrl_coal *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_NOTF_COAL;
    ctrl->command = VIRTIO_NET_CTRL_NOTF_COAL_TX_SET;
    coal->max_packets = 0;
    coal->max_usecs = 0;
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(*coal),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0094, VIRTIO_PCI_DEVICE_NET, test_net_notf_coal_zero,
              "NOTF_COAL TX_SET with zero max_packets and max_usecs",
              VIRTIO_SPEC_V1_3, "5.1.6.5", VV_QUEUE_LAST);
