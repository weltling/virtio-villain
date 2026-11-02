/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0008: vsock_wrong_src_cid
 *
 * Send a packet with src_cid different from the guest's assigned CID.
 * Spec 5.10.6: src_cid MUST be the guest's CID.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_wrong_src_cid(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    /* Use src_cid=999 which is not our assigned CID */
    pkt->src_cid = 999;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 32;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    memset(payload, 0xBB, 32);

    vv_desc(vr, 0, pkt, sizeof(*pkt), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, payload, 32, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0008, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_wrong_src_cid,
              "Packet with wrong src_cid (not guest CID)",
              VIRTIO_SPEC_V1_2, "5.10.6");
