/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0020: net_notf_coal_no_feature
 *
 * Send VIRTIO_NET_CTRL_NOTF_COAL commands without negotiating
 * VIRTIO_NET_F_NOTF_COAL. Spec v1.3 5.1.6.5: driver MUST have
 * negotiated the feature when issuing coalescing commands.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_notf_coal_no_feature(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_coal *coal =
        (struct virtio_net_ctrl_coal *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_NOTF_COAL;
    ctrl->command = VIRTIO_NET_CTRL_NOTF_COAL_TX_SET;
    coal->max_packets = 64;
    coal->max_usecs = 100;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(*coal),
                       VRING_DESC_F_NEXT, VV_QUEUE_LAST);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0020, VIRTIO_PCI_DEVICE_NET, test_net_notf_coal_no_feature,
              "NOTF_COAL command without NOTF_COAL feature",
              VIRTIO_SPEC_V1_3, "5.1.6.5", VV_QUEUE_LAST);
