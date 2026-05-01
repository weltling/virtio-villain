/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0019: net_hash_tunnel_no_feature
 *
 * Send VIRTIO_NET_CTRL_HASH_TUNNEL_SET command without negotiating
 * VIRTIO_NET_F_HASH_TUNNEL. Spec v1.3 5.1.6.5: driver MUST have
 * negotiated the feature when issuing this command.
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

#define VIRTIO_NET_CTRL_HASH_TUNNEL     7
#define VIRTIO_NET_CTRL_HASH_TUNNEL_SET 0

static test_result_t test_net_hash_tunnel_no_feature(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint32_t *tunnel_types = (uint32_t *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_HASH_TUNNEL;
    ctrl->command = VIRTIO_NET_CTRL_HASH_TUNNEL_SET;
    /* Enable all tunnel types without negotiating the feature */
    *tunnel_types = 0xFFFFFFFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(uint32_t),
                       VRING_DESC_F_NEXT, VV_QUEUE_LAST);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0019, VIRTIO_PCI_DEVICE_NET, test_net_hash_tunnel_no_feature,
              "HASH_TUNNEL_SET without HASH_TUNNEL feature",
              VIRTIO_SPEC_V1_3, "5.1.6.5", VV_QUEUE_LAST);
