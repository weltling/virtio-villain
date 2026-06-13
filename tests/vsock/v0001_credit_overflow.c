/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0001: vsock_credit_overflow
 *
 * Report buf_alloc/fwd_cnt values that cause integer overflow when
 * computing available credit. The device must handle this safely.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_credit_overflow(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;    /* guest CID */
    pkt->dst_cid = 2;    /* host CID */
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    /* Values that overflow when device computes: buf_alloc - fwd_cnt */
    pkt->buf_alloc = 0xFFFFFFFF;
    pkt->fwd_cnt = 0xFFFFFFFE;

    uint64_t pkt_phys = vv_virt_to_phys(pkt);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0001, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_credit_overflow,
              "Credit fields causing integer overflow",
              VIRTIO_SPEC_V1_2, "5.10.6");
