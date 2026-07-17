/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0016: net_offload_no_feature
 *
 * Enable a guest offload without negotiating the corresponding feature.
 * Spec 5.1.6.5.8.2: driver MUST NOT enable an offload for which the
 * appropriate feature has not been negotiated.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_offload_no_feature(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint64_t *offloads = (uint64_t *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_GUEST_OFFLOADS;
    ctrl->command = VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET;
    /* Enable all offloads without negotiating any of them */
    *offloads = (1ULL << VIRTIO_NET_F_GUEST_CSUM) |
                (1ULL << VIRTIO_NET_F_GUEST_TSO4) |
                (1ULL << VIRTIO_NET_F_GUEST_TSO6) |
                (1ULL << VIRTIO_NET_F_GUEST_ECN) |
                (1ULL << VIRTIO_NET_F_GUEST_UFO);

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(uint64_t),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0016, VIRTIO_PCI_DEVICE_NET, test_net_offload_no_feature,
              "Enable guest offloads without features",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
