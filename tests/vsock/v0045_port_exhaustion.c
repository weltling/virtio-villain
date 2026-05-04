/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0045: vsock_port_exhaustion
 *
 * Open connections to many different ports rapidly without closing.
 * Tests that the device handles port exhaustion gracefully without
 * crashing or corrupting state.
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
#define NUM_PORTS 64

static test_result_t test_vsock_port_exhaustion(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* TX queue is queue 1 for vsock */
    struct vring txvr;
    vring_alloc(&txvr, 128);
    vring_attach(dev, &txvr, 1);

    /*
     * Submit NUM_PORTS connection requests to different dst_ports,
     * each from a different src_port.
     */
    for (int i = 0; i < NUM_PORTS; i++) {
        struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

        pkt->src_cid = 3;
        pkt->dst_cid = 2;
        pkt->src_port = (uint32_t)(5000 + i);
        pkt->dst_port = (uint32_t)(8000 + i);
        pkt->len = 0;
        pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
        pkt->op = VIRTIO_VSOCK_OP_REQUEST;
        pkt->flags = 0;
        pkt->buf_alloc = 4096;
        pkt->fwd_cnt = 0;

        vring_raw_set_desc(&txvr, i, vv_virt_to_phys(pkt),
                           sizeof(*pkt), 0, 0);
        vring_raw_set_avail(&txvr, i, (uint16_t)i);
    }

    vring_raw_set_avail_idx(&txvr, NUM_PORTS);

    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    usleep(VV_TIMEOUT_MS * 1000);
    __sync_synchronize();

    uint8_t status = cfg->device_status;
    if (status == 0)
        TWEDGED("status == 0");

    /* Any completions indicate device processed without crashing */
    if (txvr.used->idx != 0)
        return TEST_PASS;

    TREJECT("no device response within timeout");
}

REGISTER_TEST(V0045, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_port_exhaustion,
              "Rapid connection requests to many ports without closing",
              VIRTIO_SPEC_V1_2, "5.10.6");
