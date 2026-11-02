/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0095: net_notf_coal_vq_get
 *
 * Send VIRTIO_NET_CTRL_NOTF_COAL_VQ_GET to read back per virtqueue
 * coalescing parameters. Spec v1.3 5.1.6.5.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_notf_coal_vq_get(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_NOTF_COAL))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_ctrl_coal_vq_get_req *req =
        (struct virtio_net_ctrl_coal_vq_get_req *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *resp_page = vv_alloc_pages(1);
    uint8_t *status = resp_page + 64;

    ctrl->class = VIRTIO_NET_CTRL_NOTF_COAL;
    ctrl->command = VIRTIO_NET_CTRL_NOTF_COAL_VQ_GET;
    req->vq_index = 0;
    req->reserved = 0;
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);

    /* Header + request data (device readable) */
    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(*req),
                       VRING_DESC_F_NEXT, 2);
    /* Response (device writable) */
    vv_desc(vr, 2, resp_page, sizeof(struct virtio_net_ctrl_coal_vq_get_resp),
            VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 3);
    /* Status (device writable) */
    vv_desc(vr, 3, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0095, VIRTIO_PCI_DEVICE_NET, test_net_notf_coal_vq_get,
              "NOTF_COAL VQ_GET readback",
              VIRTIO_SPEC_V1_3, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_NOTF_COAL), 0);
