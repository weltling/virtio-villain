/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0032: vsock_seqpacket_no_feature
 *
 * Send a packet with type=SEQPACKET without having negotiated
 * VIRTIO_VSOCK_F_SEQPACKET. Tests device handling of unsupported
 * transport types.
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

#define VIRTIO_VSOCK_TYPE_SEQPACKET 2
#define VIRTIO_VSOCK_OP_REQUEST     1

static test_result_t test_vsock_seqpacket_no_feature(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 11000;
    pkt->dst_port = 11000;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_SEQPACKET; /* without feature */
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0032, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_seqpacket_no_feature,
              "CONNECT with type=SEQPACKET without F_SEQPACKET feature",
              VIRTIO_SPEC_V1_2, "5.10.6");
