/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0048: vsock_event_queue_flood
 *
 * Post many buffers to the event virtqueue and trigger events by
 * sending connection requests on the TX queue. Tests event queue
 * handling under load.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define NUM_EVENT_BUFS 32
#define NUM_TX_PKTS 16

static test_result_t test_vsock_event_flood(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* TX queue (1) */
    struct vring txvr;
    vring_alloc(&txvr, 64);
    vring_attach(dev, &txvr, 1);

    /* Event queue (2) */
    struct vring evvr;
    vring_alloc(&evvr, 64);
    vring_attach(dev, &evvr, 2);

    /* Post many event buffers */
    for (int i = 0; i < NUM_EVENT_BUFS; i++) {
        struct virtio_vsock_event *evt = vv_alloc_pages(1);
        memset(evt, 0, sizeof(*evt));
        vring_raw_set_desc(&evvr, i, vv_virt_to_phys(evt),
                           sizeof(*evt), VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(&evvr, i, (uint16_t)i);
    }
    vring_raw_set_avail_idx(&evvr, NUM_EVENT_BUFS);

    /* Kick event queue to make buffers available to device */
    __sync_synchronize();
    virtio_pci_kick(dev, 2);

    /* Submit many TX connection requests to trigger events */
    for (int i = 0; i < NUM_TX_PKTS; i++) {
        struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

        pkt->src_cid = 3;
        pkt->dst_cid = 2;
        pkt->src_port = (uint32_t)(10000 + i);
        pkt->dst_port = (uint32_t)(20000 + i);
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
    vring_raw_set_avail_idx(&txvr, NUM_TX_PKTS);

    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    usleep(VV_TIMEOUT_MS * 1000);
    __sync_synchronize();

    uint8_t status = cfg->device_status;
    if (status == 0)
        TWEDGED("status == 0");

    /* Device survived the flood */
    if (txvr.used->idx != 0 || evvr.used->idx != 0)
        return TEST_PASS;

    TREJECT("no device response within timeout");
}

REGISTER_TEST(V0048, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_event_flood,
              "Flood event queue with buffers and trigger events",
              VIRTIO_SPEC_V1_2, "5.10.6.7");
