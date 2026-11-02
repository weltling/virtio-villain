/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0052: vsock_request_with_payload
 *
 * Send VIRTIO_VSOCK_OP_REQUEST with len set to 0x40. Spec 5.10.6.5
 * says REQUEST packets carry no payload and len must be zero. The
 * device must reset or drop the connection request without
 * crashing the host.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_request_payload(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    struct virtio_vsock_hdr *pkt = (void *)page;

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 5000;
    pkt->dst_port = 6000;
    pkt->len = 0x40;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 0x10000;
    pkt->fwd_cnt = 0;

    memset(page + sizeof(*pkt), 0xAB, 0x40);

    vv_desc(vr, 0, page, sizeof(*pkt) + 0x40, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0052, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_request_payload,
              "REQUEST packet with non zero payload length",
              VIRTIO_SPEC_V1_2, "5.10.6.5");
