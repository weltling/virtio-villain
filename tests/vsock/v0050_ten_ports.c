/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0050: ten REQUEST packets to ten distinct ports
 *
 * Spec 5.10.6 says the device must serve many concurrent streams.
 * Submit ten REQUEST packets each targeting a different host port,
 * none of which are listening, so the device should respond with
 * RST on the RX queue. Regardless of the response, the TX side
 * must drain all ten without wedging.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_vsock_ten_ports(struct virtio_dev *dev,
                                          struct vring *vr)
{
    int n = 10;
    if ((int)vr->size < n + 2)
        n = vr->size - 2;
    if (n < 4)
        return TEST_SKIP;

    for (int i = 0; i < n; i++) {
        struct virtio_vsock_hdr *h = vv_alloc_pages(1);
        h->src_cid = 3;
        h->dst_cid = 2;
        h->src_port = 30000 + i;
        h->dst_port = 40000 + i;
        h->len = 0;
        h->type = VIRTIO_VSOCK_TYPE_STREAM;
        h->op = VIRTIO_VSOCK_OP_REQUEST;
        h->flags = 0;
        h->buf_alloc = 65536;
        h->fwd_cnt = 0;

        vring_raw_set_desc(vr, i, vv_virt_to_phys(h), sizeof(*h), 0, 0);
        vring_raw_set_avail(vr, i, i);
    }
    vring_raw_set_avail_idx(vr, n);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0050, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_ten_ports,
              "ten REQUEST packets to distinct ports drained",
              VIRTIO_SPEC_V1_2, "5.10.6");
