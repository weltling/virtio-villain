/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0057: vsock_no_implied_stream_unknown_type
 *
 * Send a connection request with a completely unknown type value
 * (e.g. 0xFFFF). With or without NO_IMPLIED_STREAM, the device
 * must not crash on unrecognized type values.
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

#define VIRTIO_VSOCK_OP_REQUEST 1

static test_result_t test_vsock_unknown_type(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 0;
    pkt->type = 0xFFFF;  /* Unknown type */
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

REGISTER_TEST_Q(V0057, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_unknown_type,
                "Connect request with unknown type value",
                VIRTIO_SPEC_V1_3, "5.10.6.6.2", 0);
