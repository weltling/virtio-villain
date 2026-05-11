/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0055: vsock_no_implied_stream_connect
 *
 * Send a VIRTIO_VSOCK_OP_REQUEST with type set to a non stream
 * value (e.g. VIRTIO_VSOCK_TYPE_DGRAM or future type) when feature
 * VIRTIO_VSOCK_F_NO_IMPLIED_STREAM is NOT negotiated. Spec v1.3
 * 5.10.6.6.2: without the feature, only stream connections should
 * be accepted. Device must handle or reject gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_vsock_hdr {
    uint64_t src_cid;
    uint64_t dst_cid;
    uint32_t src_port;
    uint32_t dst_port;
    uint32_t len;
    uint16_t type;
    uint16_t op;
    uint32_t flags;
    uint32_t buf_alloc;
    uint32_t fwd_cnt;
} __attribute__((packed));

#define VIRTIO_VSOCK_OP_REQUEST   1
#define VIRTIO_VSOCK_TYPE_STREAM  1
#define VIRTIO_VSOCK_TYPE_DGRAM   3  /* Not valid without feature */

static test_result_t test_vsock_no_implied_connect(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_DGRAM;  /* Non stream type */
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt), sizeof(*pkt),
                       0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(V0055, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_no_implied_connect,
                "Connect with non stream type without NO_IMPLIED_STREAM",
                VIRTIO_SPEC_V1_3, "5.10.6.6.2", 0);
