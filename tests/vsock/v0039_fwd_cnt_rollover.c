/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0039: vsock_fwd_cnt_near_rollover
 *
 * Send a data packet with fwd_cnt set near UINT32_MAX, testing device
 * arithmetic on credit tracking near the wrap boundary.
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

static test_result_t test_vsock_fwd_cnt_rollover(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* TX on queue 1 */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = (uint8_t *)(pkt + 1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 9000;
    pkt->dst_port = 9000;
    pkt->len = 32;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0xFFFFFFF0; /* near UINT32_MAX */
    memset(payload, 0xEE, 32);

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt) + 32, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0039, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_fwd_cnt_rollover,
              "Data packet with fwd_cnt near UINT32_MAX rollover",
              VIRTIO_SPEC_V1_2, "5.10.6.3");
