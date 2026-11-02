/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0075: vsock_seqpacket_eom_eor
 *
 * Send a SEQPACKET message with EOM and EOR flags set. Spec 5.10.6.4
 * says a seqpacket message boundary is indicated by EOM flag, and
 * EOR indicates end of record. The device must process the packet
 * without crashing when both boundary flags are set.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_seqpacket_eom_eor(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_VSOCK_F_SEQPACKET))
        return TEST_SKIP;

    /* CONNECT first to establish a seqpacket connection */
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3;
    conn->dst_cid = 2;
    conn->src_port = 7000;
    conn->dst_port = 7000;
    conn->len = 0;
    conn->type = VIRTIO_VSOCK_TYPE_SEQPACKET;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0;
    conn->buf_alloc = 65536;
    conn->fwd_cnt = 0;

    vv_desc(vr, 0, conn, sizeof(*conn), 0, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    /* RW with EOM + EOR flags */
    struct virtio_vsock_hdr *rw = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    rw->src_cid = 3;
    rw->dst_cid = 2;
    rw->src_port = 7000;
    rw->dst_port = 7000;
    rw->len = 32;
    rw->type = VIRTIO_VSOCK_TYPE_SEQPACKET;
    rw->op = VIRTIO_VSOCK_OP_RW;
    rw->flags = VIRTIO_VSOCK_SEQ_EOM | VIRTIO_VSOCK_SEQ_EOR;
    rw->buf_alloc = 65536;
    rw->fwd_cnt = 0;

    memset(payload, 0x42, 32);

    vv_desc(vr, 0, rw, sizeof(*rw), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, payload, 32, 0, 0);

    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(V0075, VIRTIO_PCI_DEVICE_VSOCK,
              test_vsock_seqpacket_eom_eor,
              "SEQPACKET RW with EOM and EOR flags",
              VIRTIO_SPEC_V1_2, "5.10.6.4",
              (1ULL << VIRTIO_VSOCK_F_SEQPACKET), 0);
