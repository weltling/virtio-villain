/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0047: vsock_dgram_type
 *
 * Send a packet with type=VIRTIO_VSOCK_TYPE_DGRAM (value 3).
 * Tests device handling of datagram type which may not be
 * supported, expecting graceful rejection.
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

#define VIRTIO_VSOCK_TYPE_DGRAM   3
#define VIRTIO_VSOCK_OP_RW        5

static test_result_t test_vsock_dgram_type(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* TX queue is queue 1 */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 6000;
    pkt->dst_port = 6000;
    pkt->len = 64;
    pkt->type = VIRTIO_VSOCK_TYPE_DGRAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    memset(payload, 0xEE, 64);

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(pkt), sizeof(*pkt),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(payload), 64, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    usleep(VV_TIMEOUT_MS * 1000);
    __sync_synchronize();

    uint8_t status = cfg->device_status;
    if (status == 0)
        TWEDGED("status == 0");

    if (txvr.used->idx != 0)
        return TEST_PASS;

    TREJECT("no device response within timeout");
}

REGISTER_TEST(V0047, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_dgram_type,
              "Send packet with type=VIRTIO_VSOCK_TYPE_DGRAM",
              VIRTIO_SPEC_V1_2, "5.10.6");
