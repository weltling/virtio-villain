/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0020: vsock_data_empty_payload
 *
 * Send a DATA packet with len=0 (empty payload). Some implementations
 * may not handle zero-length data transfers gracefully.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_data_empty(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 5000;
    pkt->dst_port = 5000;
    pkt->len = 0; /* no payload */
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vv_desc(vr, 0, pkt, sizeof(*pkt), 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0020, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_data_empty,
              "DATA packet with len=0 (empty payload)",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
