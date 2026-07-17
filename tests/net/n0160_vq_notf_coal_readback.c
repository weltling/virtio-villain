/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0160: net_vq_notf_coal_set_get_readback
 *
 * Spec 5.1.6.5.6: the VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET and
 * VIRTIO_NET_CTRL_NOTF_COAL_VQ_GET commands require the
 * VIRTIO_NET_F_VQ_NOTF_COAL feature (not plain NOTF_COAL). Set
 * per virtqueue coalescing parameters for vq 0, then read them
 * back with VQ_GET and verify the device returns the same values.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define COAL_MAX_PACKETS 16
#define COAL_MAX_USECS   250

static test_result_t test_net_vq_coal_readback(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_VQ_NOTF_COAL))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_coal_vq *cvq =
        (struct virtio_net_ctrl_coal_vq *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);
    uint16_t ai = 0;

    /* SET coalescing parameters for virtqueue 0. */
    ctrl->class = VIRTIO_NET_CTRL_NOTF_COAL;
    ctrl->command = VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET;
    cvq->vq_index = 0;
    cvq->reserved = 0;
    cvq->max_packets = COAL_MAX_PACKETS;
    cvq->max_usecs = COAL_MAX_USECS;
    *status = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(cvq), sizeof(*cvq),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*status != VIRTIO_NET_OK) TFAIL("VQ_SET status %u", *status);

    /* GET them back and verify they match. */
    struct virtio_net_ctrl_coal_vq_get_req *req =
        (struct virtio_net_ctrl_coal_vq_get_req *)((uint8_t *)ctrl + sizeof(*ctrl));
    struct virtio_net_ctrl_coal_vq_get_resp *resp = vv_alloc_pages(1);
    ctrl->class = VIRTIO_NET_CTRL_NOTF_COAL;
    ctrl->command = VIRTIO_NET_CTRL_NOTF_COAL_VQ_GET;
    req->vq_index = 0;
    req->reserved = 0;
    memset(resp, 0, sizeof(*resp));

    *status = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, ai++, 0);
    vring_raw_set_avail_idx(vr, ai);
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*status != VIRTIO_NET_OK) TFAIL("VQ_GET status %u", *status);

    if (resp->max_packets != COAL_MAX_PACKETS)
        TFAIL("max_packets %u, expected %u", resp->max_packets,
              COAL_MAX_PACKETS);
    if (resp->max_usecs != COAL_MAX_USECS)
        TFAIL("max_usecs %u, expected %u", resp->max_usecs, COAL_MAX_USECS);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0160, VIRTIO_PCI_DEVICE_NET, test_net_vq_coal_readback,
              "VQ_NOTF_COAL set then get returns the same parameters",
              VIRTIO_SPEC_V1_4, "5.1.6.5.6", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_VQ_NOTF_COAL), 0);
