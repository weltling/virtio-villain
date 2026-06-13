/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0004: vsock_double_connect
 *
 * Send two CONNECT requests for the same (src_port, dst_port) pair.
 * The second must get an error rather than creating duplicate state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_double_connect(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_vsock_hdr *pkt1 = vv_alloc_pages(1);
    struct virtio_vsock_hdr *pkt2 = vv_alloc_pages(1);

    /* First CONNECT */
    pkt1->src_cid = 3;
    pkt1->dst_cid = 2;
    pkt1->src_port = 1234;
    pkt1->dst_port = 5678;
    pkt1->len = 0;
    pkt1->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt1->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt1->flags = 0;
    pkt1->buf_alloc = 4096;
    pkt1->fwd_cnt = 0;

    /* Second CONNECT - same ports */
    memcpy(pkt2, pkt1, sizeof(*pkt2));

    uint64_t pkt1_phys = vv_virt_to_phys(pkt1);
    uint64_t pkt2_phys = vv_virt_to_phys(pkt2);

    /* Submit both as separate descriptors */
    vring_raw_set_desc(vr, 0, pkt1_phys, sizeof(*pkt1), 0, 0);
    vring_raw_set_desc(vr, 1, pkt2_phys, sizeof(*pkt2), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0004, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_double_connect,
              "Duplicate CONNECT for same port pair",
              VIRTIO_SPEC_V1_2, "5.10.6");
