/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0043: Concurrent TX and event queue operations (spec 5.10.6)
 *
 * Post buffers on event queue and TX queue simultaneously, then
 * kick both. Tests device handling of concurrent queue processing
 * across different vsock queue types.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_concurrent_tx_event(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 3)
        return TEST_SKIP;

    /* TX queue 1 */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    /* Event queue 2 */
    struct vring evvr;
    vring_alloc(&evvr, 16);
    vring_attach(dev, &evvr, 2);

    /* Set up TX packet */
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 7000;
    pkt->dst_port = 7000;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt), 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    /* Set up event buffer */
    struct virtio_vsock_event *evt = vv_alloc_pages(1);
    memset(evt, 0, sizeof(*evt));

    vring_raw_set_desc(&evvr, 0, vv_virt_to_phys(evt), sizeof(*evt),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&evvr, 0, 0);
    vring_raw_set_avail_idx(&evvr, 1);

    /* Kick both queues back-to-back */
    __sync_synchronize();
    virtio_pci_kick(dev, 1);
    virtio_pci_kick(dev, 2);

    usleep(VV_TIMEOUT_MS * 1000);

    /* Check if either produced a response */
    __sync_synchronize();
    uint8_t status = cfg->device_status;
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST(V0043, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_concurrent_tx_event,
              "Concurrent TX and event queue operations",
              VIRTIO_SPEC_V1_2, "5.10.6");
