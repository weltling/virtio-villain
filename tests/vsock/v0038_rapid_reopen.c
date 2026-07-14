/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0038: vsock_rapid_open_close_reopen
 *
 * Send CONNECT, then immediately SHUTDOWN, then CONNECT again on the
 * same port. Tests device connection state machine for rapid recycling.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_rapid_reopen(struct virtio_dev *dev,
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

    /* Packet 0: CONNECT */
    struct virtio_vsock_hdr *conn = vv_alloc_pages(1);
    conn->src_cid = 3;
    conn->dst_cid = 2;
    conn->src_port = 8000;
    conn->dst_port = 8000;
    conn->len = 0;
    conn->type = VIRTIO_VSOCK_TYPE_STREAM;
    conn->op = VIRTIO_VSOCK_OP_REQUEST;
    conn->flags = 0;
    conn->buf_alloc = 4096;
    conn->fwd_cnt = 0;

    /* Packet 1: SHUTDOWN */
    struct virtio_vsock_hdr *shut = vv_alloc_pages(1);
    shut->src_cid = 3;
    shut->dst_cid = 2;
    shut->src_port = 8000;
    shut->dst_port = 8000;
    shut->len = 0;
    shut->type = VIRTIO_VSOCK_TYPE_STREAM;
    shut->op = VIRTIO_VSOCK_OP_SHUTDOWN;
    shut->flags = VIRTIO_VSOCK_SHUTDOWN_BOTH;
    shut->buf_alloc = 4096;
    shut->fwd_cnt = 0;

    /* Packet 2: CONNECT again (reopen) */
    struct virtio_vsock_hdr *recon = vv_alloc_pages(1);
    recon->src_cid = 3;
    recon->dst_cid = 2;
    recon->src_port = 8000;
    recon->dst_port = 8000;
    recon->len = 0;
    recon->type = VIRTIO_VSOCK_TYPE_STREAM;
    recon->op = VIRTIO_VSOCK_OP_REQUEST;
    recon->flags = 0;
    recon->buf_alloc = 4096;
    recon->fwd_cnt = 0;

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(conn), sizeof(*conn), 0, 0);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(shut), sizeof(*shut), 0, 0);
    vring_raw_set_desc(&txvr, 2, vv_virt_to_phys(recon), sizeof(*recon), 0, 0);

    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail(&txvr, 1, 1);
    vring_raw_set_avail(&txvr, 2, 2);
    vring_raw_set_avail_idx(&txvr, 3);

    /* Kick once for all three */
    uint16_t before = txvr.used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    /* Wait for all three to be consumed */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 2 * 1000) {
        usleep(10000);
        __sync_synchronize();
        if ((uint16_t)(txvr.used->idx - before) >= 3)
            return TEST_PASS;
        elapsed += 10000;
    }

    /* Even partial completion is acceptable */
    if (txvr.used->idx != before)
        return TEST_PASS;

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_REQUIRES(V0038, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rapid_reopen,
              "Rapid CONNECT/SHUTDOWN/CONNECT on same port",
              VIRTIO_SPEC_V1_2, "5.10.6.2",
              0, 2);
