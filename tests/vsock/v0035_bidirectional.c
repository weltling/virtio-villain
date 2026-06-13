/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0035: vsock_bidirectional_simultaneous
 *
 * Post RX buffers on the RX queue and send TX data simultaneously
 * on the same connection. Tests device handling of full-duplex
 * data flow with concurrent queue processing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_bidirectional(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    /* vsock needs at least 3 queues: RX(0), TX(1), event(2) */
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* Post RX buffers on queue 0 (already vr) */
    uint8_t *rxbuf = vv_alloc_pages(1);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rxbuf),
                       sizeof(struct virtio_vsock_hdr) + 128,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    __sync_synchronize();
    virtio_pci_kick(dev, 0);

    /* Set up TX queue (queue 1) and send data */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_vsock_hdr *txpkt = vv_alloc_pages(1);
    uint8_t *payload = (uint8_t *)(txpkt + 1);

    txpkt->src_cid = 3;
    txpkt->dst_cid = 2;
    txpkt->src_port = 5000;
    txpkt->dst_port = 5000;
    txpkt->len = 64;
    txpkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    txpkt->op = VIRTIO_VSOCK_OP_RW;
    txpkt->flags = 0;
    txpkt->buf_alloc = 4096;
    txpkt->fwd_cnt = 0;
    memset(payload, 0xAB, 64);

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(txpkt),
                       sizeof(*txpkt) + 64, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    /* Kick TX - device should consume it */
    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0035, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_bidirectional,
              "Bidirectional RX/TX on same vsock connection",
              VIRTIO_SPEC_V1_2, "5.10.6");
