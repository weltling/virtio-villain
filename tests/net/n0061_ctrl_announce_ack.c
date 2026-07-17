/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0061: CTRL_ANNOUNCE ACK command (spec 5.1.6.5.5)
 *
 * Issue CTRL_ANNOUNCE with ACK command. Without the feature
 * being negotiated, the device should reject or ignore.
 * Tests that the device handles the announce protocol gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_ctrl_announce_ack(struct virtio_dev *dev,
                                            struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_GUEST_ANNOUNCE))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_ANNOUNCE;
    ctrl->command = VIRTIO_NET_CTRL_ANNOUNCE_ACK;
    *ack = 0xFF;

    /* ANNOUNCE has no data payload, just header + ack */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0061, VIRTIO_PCI_DEVICE_NET, test_ctrl_announce_ack,
              "CTRL_ANNOUNCE ACK command processing",
              VIRTIO_SPEC_V1_2, "5.1.6.5.5",
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE), 0);
