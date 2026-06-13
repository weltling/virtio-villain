/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0025: vsock_impersonate_host_cid
 *
 * Send a packet with src_cid set to the host CID (2) instead of our
 * guest CID (3). The device must detect the CID mismatch and not
 * forward the packet as if it came from the host.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_impersonate_host(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 2;  /* HOST CID - impersonation! */
    pkt->dst_cid = 2;  /* also host - loopback attempt */
    pkt->src_port = 1234;
    pkt->dst_port = 1234;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    uint64_t pkt_phys = vv_virt_to_phys(pkt);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0025, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_impersonate_host,
              "Packet with src_cid=HOST (CID impersonation)",
              VIRTIO_SPEC_V1_2, "5.10.6");
