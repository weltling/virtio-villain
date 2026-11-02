/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0015: vsock_len_exceeds_payload
 *
 * Submit a vsock packet where the len field in the header claims more
 * payload bytes than the actual descriptor provides. The device may
 * attempt to read beyond the descriptor boundary, causing an OOB read.
 */
#include "lib/virtio_test.h"

static test_result_t test_vsock_len_exceeds_payload(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 4096; /* Claims 4096 bytes of payload */
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 65536;
    pkt->fwd_cnt = 0;

    memset(payload, 'A', 64);

    uint64_t pkt_phys = vv_virt_to_phys(pkt);

    /* Header says len=4096, but payload descriptor is only 64 bytes */
    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, payload, 64, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(V0015, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_len_exceeds_payload,
              "len field larger than actual payload descriptor",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
