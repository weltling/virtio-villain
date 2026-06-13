/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0011: vsock_starve_rx
 *
 * Submit TX packets without providing any RX buffers. Some VMMs may
 * block or crash if RX queue is empty when they need to send a
 * response (RST, CREDIT_UPDATE).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_starve_rx(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    /*
     * Send a CONNECTION_REQUEST without providing any RX buffers.
     * The device needs to send back RESPONSE or RST but has nowhere
     * to put it. Should not crash.
     */
    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 7777;
    pkt->dst_port = 8888;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    uint64_t pkt_phys = vv_virt_to_phys(pkt);

    /* Only TX queue - no RX buffers provided */
    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0011, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_starve_rx,
              "TX without RX buffers (starved RX queue)",
              VIRTIO_SPEC_V1_2, "5.10.6");
