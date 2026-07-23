/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0166: net_vq_notf_coal_no_feature
 *
 * Send a VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET command without
 * negotiating VIRTIO_NET_F_VQ_NOTF_COAL (bit 52). Spec 5.1.6.5
 * Notifications Coalescing: the per virtqueue coalescing commands
 * VQ_SET and VQ_GET require VQ_NOTF_COAL, which is a distinct
 * feature from NOTF_COAL (bit 53) that gates the TX_SET and RX_SET
 * commands. n0020 covers TX_SET without NOTF_COAL; this covers the
 * per virtqueue command without its own feature bit. The device
 * must reject it by acking VIRTIO_NET_ERR. The control queue needs
 * CTRL_VQ, so require it and skip when it is not offered. Skip when
 * the device offers VQ_NOTF_COAL, since then the command is valid.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_net_vq_notf_coal_no_feature(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;
    if (virtio_pci_feature_offered(dev, VIRTIO_NET_F_VQ_NOTF_COAL))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_coal_vq *cvq =
        (struct virtio_net_ctrl_coal_vq *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_NOTF_COAL;
    ctrl->command = VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET;
    cvq->vq_index = 0;
    cvq->reserved = 0;
    cvq->max_packets = 16;
    cvq->max_usecs = 250;
    *ack = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(*cvq),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*ack != VIRTIO_NET_ERR)
        TFAIL("device acked 0x%02x, expected VIRTIO_NET_ERR for a per "
              "virtqueue coalescing command without VQ_NOTF_COAL", *ack);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0166, VIRTIO_PCI_DEVICE_NET,
                         test_net_vq_notf_coal_no_feature,
                         "per virtqueue coalescing command without feature",
                         VIRTIO_SPEC_V1_4, "5.1.6.5", VV_QUEUE_LAST,
                         (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
