/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0021: vsock_credit_update_zero_buf
 *
 * Send a CREDIT_UPDATE with buf_alloc=0 after a connection context
 * might exist. This tells the peer we have no buffer space; the device
 * must handle zero-credit updates without division errors.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_credit_zero(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 6000;
    pkt->dst_port = 6000;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_CREDIT_UPDATE;
    pkt->flags = 0;
    pkt->buf_alloc = 0; /* zero buffer space */
    pkt->fwd_cnt = 0;

    uint64_t pkt_phys = vv_virt_to_phys(pkt);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0021, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_credit_zero,
              "CREDIT_UPDATE with buf_alloc=0 (no space)",
              VIRTIO_SPEC_V1_2, "5.10.6.5");
