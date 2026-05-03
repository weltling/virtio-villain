/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0042: TX with oversized header descriptor (spec 5.10.6)
 *
 * Send a vsock packet with the header descriptor length set
 * much larger than sizeof(virtio_vsock_hdr). The extra bytes
 * could be interpreted as payload. Tests boundary handling.
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
#define VIRTIO_VSOCK_OP_REQUEST   1

static test_result_t test_vsock_oversized_hdr(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    /* Allocate full page - header at start, rest is "extra" */
    uint8_t *buf = vv_alloc_pages(1);
    struct virtio_vsock_hdr *pkt = (struct virtio_vsock_hdr *)buf;

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 5000;
    pkt->dst_port = 5000;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    /* Fill rest of page with garbage */
    memset(buf + sizeof(*pkt), 0xDD, PAGE_SIZE - sizeof(*pkt));

    /* Descriptor len = 4096 (way larger than header) */
    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(buf), PAGE_SIZE, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0042, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_oversized_hdr,
              "TX vsock with oversized descriptor (4096 byte header)",
              VIRTIO_SPEC_V1_2, "5.10.6");
