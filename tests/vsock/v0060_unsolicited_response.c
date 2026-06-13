/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0060: vsock_response_without_request
 *
 * Send RESPONSE without a preceding REQUEST on an unconnected
 * port. Spec 5.10.6.5 says RESPONSE must follow a REQUEST from
 * the device. An unsolicited RESPONSE must be silently ignored
 * without crashing the host.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_unsolicited_response(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    struct virtio_vsock_hdr *pkt = (void *)page;
    memset(page, 0, 4096);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 11000;
    pkt->dst_port = 12000;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RESPONSE;
    pkt->flags = 0;
    pkt->buf_alloc = 0x10000;
    pkt->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(page),
                       sizeof(*pkt), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0060, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_unsolicited_response,
              "RESPONSE without preceding REQUEST",
              VIRTIO_SPEC_V1_2, "5.10.6.5");
