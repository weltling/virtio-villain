/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0077: seqpacket fragmented message across two packets.
 *
 * Spec 5.10.6.4: A seqpacket message may span multiple packets.
 * Only the final packet carries the EOM flag. Send two RW packets
 * on the same connection: the first without EOM, the second with
 * EOM set. The device must consume both as a single logical message.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_seqpacket_frag(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_VSOCK_F_SEQPACKET))
        return TEST_SKIP;

    /* CONNECT */
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3; conn->dst_cid = 2;
    conn->src_port = 8000; conn->dst_port = 8000;
    conn->len = 0;
    conn->type = VIRTIO_VSOCK_TYPE_SEQPACKET;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0;
    conn->buf_alloc = 65536; conn->fwd_cnt = 0;

    vv_desc(vr, 0, conn, sizeof(*conn), 0, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    /* Fragment 1: no EOM */
    struct virtio_vsock_hdr *frag1 = vv_alloc_pages(1);
    uint8_t *p1 = vv_alloc_pages(1);
    frag1->src_cid = 3; frag1->dst_cid = 2;
    frag1->src_port = 8000; frag1->dst_port = 8000;
    frag1->len = 16;
    frag1->type = VIRTIO_VSOCK_TYPE_SEQPACKET;
    frag1->op = VIRTIO_VSOCK_OP_RW;
    frag1->flags = 0;  /* no EOM */
    frag1->buf_alloc = 65536; frag1->fwd_cnt = 0;
    memset(p1, 'A', 16);

    /* Fragment 2: EOM + EOR */
    struct virtio_vsock_hdr *frag2 = vv_alloc_pages(1);
    uint8_t *p2 = vv_alloc_pages(1);
    frag2->src_cid = 3; frag2->dst_cid = 2;
    frag2->src_port = 8000; frag2->dst_port = 8000;
    frag2->len = 16;
    frag2->type = VIRTIO_VSOCK_TYPE_SEQPACKET;
    frag2->op = VIRTIO_VSOCK_OP_RW;
    frag2->flags = VIRTIO_VSOCK_SEQ_EOM | VIRTIO_VSOCK_SEQ_EOR;
    frag2->buf_alloc = 65536; frag2->fwd_cnt = 16;
    memset(p2, 'B', 16);

    /* Submit both fragments */
    vv_desc(vr, 0, frag1, sizeof(*frag1), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, p1, 16, 0, 0);
    vv_desc(vr, 2, frag2, sizeof(*frag2), VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, p2, 16, 0, 0);

    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail(vr, 2, 2);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(V0077, VIRTIO_PCI_DEVICE_VSOCK,
              test_vsock_seqpacket_frag,
              "SEQPACKET fragmented message with EOM on last packet",
              VIRTIO_SPEC_V1_2, "5.10.6.4",
              (1ULL << VIRTIO_VSOCK_F_SEQPACKET), 0);
