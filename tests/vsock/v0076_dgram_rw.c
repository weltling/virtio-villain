/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0076: vsock dgram RW data send.
 *
 * When VIRTIO_VSOCK_F_DGRAM is negotiated, the driver may send
 * connectionless datagrams using OP_RW with type=DGRAM. Unlike
 * stream sockets, no CONNECT handshake is needed. The device must
 * consume the packet without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_dgram_rw(struct virtio_dev *dev,
                                         struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_VSOCK_F_DGRAM))
        return TEST_SKIP;

    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 9000;
    pkt->dst_port = 9000;
    pkt->len = 32;
    pkt->type = VIRTIO_VSOCK_TYPE_DGRAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 0;  /* no credit for dgram */
    pkt->fwd_cnt = 0;

    memset(payload, 0x42, 32);

    vv_desc(vr, 0, pkt, sizeof(*pkt), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, payload, 32, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(V0076, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_dgram_rw,
              "DGRAM connectionless data send",
              VIRTIO_SPEC_V1_4, "5.10.6",
              (1ULL << VIRTIO_VSOCK_F_DGRAM), 0);
