/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0064: vsock OP_RST for a connection that was never opened.
 *
 * Spec 5.10.6.6: OP_RST tears down a connection. Send an
 * OP_RST for a fresh src_port/dst_port pair with no preceding
 * OP_REQUEST or OP_RESPONSE. The device must drop the spurious
 * reset without crashing or allocating per connection state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_rst_unknown(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    memset(pkt, 0, sizeof(*pkt));

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 0xABCD;
    pkt->dst_port = 0x1234;
    pkt->len  = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op   = VIRTIO_VSOCK_OP_RST;
    pkt->flags = 0;
    pkt->buf_alloc = 0x10000;
    pkt->fwd_cnt   = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt), sizeof(*pkt), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0064, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rst_unknown,
              "OP_RST for a never opened connection",
              VIRTIO_SPEC_V1_2, "5.10.6.6");
