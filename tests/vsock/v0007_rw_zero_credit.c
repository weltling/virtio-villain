/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0007: vsock_rw_zero_credit
 *
 * Send an RW (data) packet when the peer has advertised zero credit.
 * Spec 5.10.6: driver MUST NOT send data when peer buf_alloc minus
 * peer fwd_cnt is zero.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_rw_zero_credit(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    /* Send data with buf_alloc=0, meaning peer has no room */
    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 128;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 0;  /* zero credit advertised */
    pkt->fwd_cnt = 0;

    memset(payload, 0xAA, 128);

    uint64_t pkt_phys = vv_virt_to_phys(pkt);
    uint64_t payload_phys = vv_virt_to_phys(payload);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, payload_phys, 128, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0007, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rw_zero_credit,
              "RW packet with zero peer credit",
              VIRTIO_SPEC_V1_2, "5.10.6");
