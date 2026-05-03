/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0028: vsock_shutdown_then_rst
 *
 * Send SHUTDOWN immediately followed by RST on the same (non-existent)
 * stream. Tests the device's handling of rapid teardown sequences
 * where both endpoints race to close, potentially causing double-free
 * or use-after-free of connection state.
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

#define VIRTIO_VSOCK_TYPE_STREAM  1
#define VIRTIO_VSOCK_OP_SHUTDOWN  4
#define VIRTIO_VSOCK_OP_RST       5

static test_result_t test_vsock_shutdown_then_rst(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_vsock_hdr *pkt0 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *pkt1 = vv_alloc_pages(1);

    /* SHUTDOWN */
    pkt0->src_cid = 3;
    pkt0->dst_cid = 2;
    pkt0->src_port = 7777;
    pkt0->dst_port = 7777;
    pkt0->len = 0;
    pkt0->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt0->op = VIRTIO_VSOCK_OP_SHUTDOWN;
    pkt0->flags = 3; /* SHUTDOWN_RD | SHUTDOWN_WR */
    pkt0->buf_alloc = 0;
    pkt0->fwd_cnt = 0;

    /* RST immediately after */
    pkt1->src_cid = 3;
    pkt1->dst_cid = 2;
    pkt1->src_port = 7777;
    pkt1->dst_port = 7777;
    pkt1->len = 0;
    pkt1->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt1->op = VIRTIO_VSOCK_OP_RST;
    pkt1->flags = 0;
    pkt1->buf_alloc = 0;
    pkt1->fwd_cnt = 0;

    uint64_t p0_phys = vv_virt_to_phys(pkt0);
    uint64_t p1_phys = vv_virt_to_phys(pkt1);

    /* Two descriptors in the avail ring - processed back to back */
    vring_raw_set_desc(vr, 0, p0_phys, sizeof(*pkt0), 0, 0);
    vring_raw_set_desc(vr, 1, p1_phys, sizeof(*pkt1), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0028, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_shutdown_then_rst,
              "SHUTDOWN immediately followed by RST on same stream",
              VIRTIO_SPEC_V1_2, "5.10.6.6");
