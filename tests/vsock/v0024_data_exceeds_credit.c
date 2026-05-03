/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0024: vsock_data_exceeds_peer_credit
 *
 * Send a DATA packet with len=65536 in a single descriptor (far
 * exceeding any reasonable buf_alloc). Tests that the device enforces
 * credit limits without buffer overflows.
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
#define VIRTIO_VSOCK_OP_RW       5

static test_result_t test_vsock_data_over_credit(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    /* Allocate header + large payload area */
    uint8_t *buf = vv_alloc_pages(32); /* 128 KiB */
    struct virtio_vsock_hdr *pkt = (struct virtio_vsock_hdr *)buf;

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 4000;
    pkt->dst_port = 4000;
    pkt->len = 65536; /* claims 64K payload */
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    /* Fill payload area */
    memset(buf + sizeof(*pkt), 0xBB, 65536);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Single descriptor: header + 64K payload */
    vring_raw_set_desc(vr, 0, buf_phys, sizeof(*pkt) + 65536, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0024, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_data_over_credit,
              "DATA packet exceeding peer buf_alloc (64K)",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
