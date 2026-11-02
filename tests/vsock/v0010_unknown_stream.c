/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0010: vsock_unknown_stream
 *
 * Send data on a stream that was never connected (no prior REQUEST/
 * RESPONSE handshake). The device should reject it.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_unknown_stream(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    /* Send data on port 9999 which was never established */
    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 9999;
    pkt->dst_port = 9999;
    pkt->len = 64;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    memset(payload, 0xDD, 64);

    vv_desc(vr, 0, pkt, sizeof(*pkt), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, payload, 64, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0010, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_unknown_stream,
              "Data on unconnected stream",
              VIRTIO_SPEC_V1_2, "5.10.6");
