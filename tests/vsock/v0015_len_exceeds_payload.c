/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0015: vsock_len_exceeds_payload
 *
 * Submit a vsock packet where the len field in the header claims more
 * payload bytes than the actual descriptor provides. The device may
 * attempt to read beyond the descriptor boundary, causing an OOB read.
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
#define VIRTIO_VSOCK_OP_RW        5

static test_result_t test_vsock_len_exceeds_payload(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 1234;
    pkt->dst_port = 5678;
    pkt->len = 4096; /* Claims 4096 bytes of payload */
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 65536;
    pkt->fwd_cnt = 0;

    memset(payload, 'A', 64);

    uint64_t pkt_phys = vv_virt_to_phys(pkt);
    uint64_t payload_phys = vv_virt_to_phys(payload);

    /* Header says len=4096, but payload descriptor is only 64 bytes */
    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, payload_phys, 64, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0015, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_len_exceeds_payload,
              "len field larger than actual payload descriptor",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
