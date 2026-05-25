/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0065: vsock OP_SHUTDOWN sent after OP_REQUEST and before any
 * OP_RESPONSE arrives.
 *
 * Spec 5.10.6.6: A connection is established only after the peer
 * sends OP_RESPONSE. Submit OP_REQUEST then OP_SHUTDOWN(both)
 * targeting the same endpoints in the same avail batch. The
 * device must tear down the half open attempt without leaking
 * per connection state and without acknowledging a connection
 * that was never accepted.
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

#define VIRTIO_VSOCK_TYPE_STREAM 1
#define VIRTIO_VSOCK_OP_REQUEST  1
#define VIRTIO_VSOCK_OP_SHUTDOWN 4
#define VIRTIO_VSOCK_FLAG_SHUTDOWN_SEND 1
#define VIRTIO_VSOCK_FLAG_SHUTDOWN_RECV 2

static void fill(struct virtio_vsock_hdr *p, uint16_t op, uint32_t flags)
{
    memset(p, 0, sizeof(*p));
    p->src_cid = 3;
    p->dst_cid = 2;
    p->src_port = 0xCAFE;
    p->dst_port = 0xFEED;
    p->len = 0;
    p->type = VIRTIO_VSOCK_TYPE_STREAM;
    p->op = op;
    p->flags = flags;
    p->buf_alloc = 0x10000;
    p->fwd_cnt = 0;
}

static test_result_t test_vsock_shutdown_before_response(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    struct virtio_vsock_hdr *req = vv_alloc_pages(1);
    struct virtio_vsock_hdr *sd  = vv_alloc_pages(1);

    fill(req, VIRTIO_VSOCK_OP_REQUEST, 0);
    fill(sd,  VIRTIO_VSOCK_OP_SHUTDOWN,
         VIRTIO_VSOCK_FLAG_SHUTDOWN_SEND | VIRTIO_VSOCK_FLAG_SHUTDOWN_RECV);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req), 0, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(sd),  sizeof(*sd),  0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0065, VIRTIO_PCI_DEVICE_VSOCK,
              test_vsock_shutdown_before_response,
              "OP_SHUTDOWN sent before peer OP_RESPONSE arrives",
              VIRTIO_SPEC_V1_2, "5.10.6.6");
