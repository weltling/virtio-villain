/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0049: send three packets back to back on the TX queue
 *
 * Spec 5.10.6 says the device must accept multiple packets
 * submitted on the TX queue. None of these packets target a real
 * connection, so the device should generate RST replies on the
 * RX queue, but the TX side must drain every packet without
 * wedging. This catches devices that block after a single packet.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

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

#define VIRTIO_VSOCK_TYPE_STREAM  1
#define VIRTIO_VSOCK_OP_REQUEST   1
#define VIRTIO_VSOCK_OP_SHUTDOWN  4
#define VIRTIO_VSOCK_OP_RST       5

static void fill(struct virtio_vsock_hdr *h, uint16_t op, uint32_t port)
{
    h->src_cid = 3;
    h->dst_cid = 2;
    h->src_port = port;
    h->dst_port = port;
    h->len = 0;
    h->type = VIRTIO_VSOCK_TYPE_STREAM;
    h->op = op;
    h->flags = (op == VIRTIO_VSOCK_OP_SHUTDOWN) ? 3 : 0;
    h->buf_alloc = 65536;
    h->fwd_cnt = 0;
}

static test_result_t test_vsock_three_pkts(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_vsock_hdr *p0 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *p1 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *p2 = vv_alloc_pages(1);

    fill(p0, VIRTIO_VSOCK_OP_REQUEST, 9001);
    fill(p1, VIRTIO_VSOCK_OP_SHUTDOWN, 9001);
    fill(p2, VIRTIO_VSOCK_OP_RST, 9001);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(p0), sizeof(*p0), 0, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(p1), sizeof(*p1), 0, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(p2), sizeof(*p2), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail(vr, 2, 2);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0049, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_three_pkts,
              "REQUEST then SHUTDOWN then RST drained from TX",
              VIRTIO_SPEC_V1_2, "5.10.6");
