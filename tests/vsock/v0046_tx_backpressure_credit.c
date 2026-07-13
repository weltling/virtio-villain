/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0046: vsock_tx_backpressure_credit
 *
 * Send data until credit is exhausted, then send one more packet.
 * The device must handle the over-credit send gracefully (reject
 * or drop, but not crash).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define PAYLOAD_SIZE 4096
#define NUM_SENDS 32

static test_result_t test_vsock_backpressure(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* TX queue is queue 1 */
    struct vring txvr;
    vring_alloc(&txvr, 128);
    vring_attach(dev, &txvr, 1);

    /*
     * Send NUM_SENDS packets of PAYLOAD_SIZE each. The peer's
     * buf_alloc is unknown but we report fwd_cnt=0 and buf_alloc=0
     * so from the peer's perspective we have no receive credit.
     * We intentionally exceed any reasonable credit window.
     */
    for (int i = 0; i < NUM_SENDS; i++) {
        struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
        uint8_t *payload = vv_alloc_pages(1);

        pkt->src_cid = 3;
        pkt->dst_cid = 2;
        pkt->src_port = 9000;
        pkt->dst_port = 9000;
        pkt->len = PAYLOAD_SIZE;
        pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
        pkt->op = VIRTIO_VSOCK_OP_RW;
        pkt->flags = 0;
        pkt->buf_alloc = 0; /* claim no buffer space */
        pkt->fwd_cnt = 0;

        memset(payload, 0xDD, PAYLOAD_SIZE);

        uint16_t base = (uint16_t)(i * 2);
        vring_raw_set_desc(&txvr, base, vv_virt_to_phys(pkt),
                           sizeof(*pkt), VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(&txvr, base + 1, vv_virt_to_phys(payload),
                           PAYLOAD_SIZE, 0, 0);
        vring_raw_set_avail(&txvr, i, base);
    }

    vring_raw_set_avail_idx(&txvr, NUM_SENDS);

    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    usleep(VV_TIMEOUT_MS * 1000);
    __sync_synchronize();

    uint8_t status = cfg->device_status;
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_REQUIRES(V0046, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_backpressure,
              "Send data exceeding credit then one more packet",
              VIRTIO_SPEC_V1_2, "5.10.6.3",
              0, 3);
