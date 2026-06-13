/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0034: vsock_src_cid_any
 *
 * Send a CONNECT with src_cid = VMADDR_CID_ANY (0xFFFFFFFF / -1U).
 * This is a wildcard CID that has special meaning in the vsock
 * address family. The device must reject or handle it appropriately.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VMADDR_CID_ANY           0xFFFFFFFF

static test_result_t test_vsock_src_cid_any(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = VMADDR_CID_ANY; /* wildcard */
    pkt->dst_cid = 2;
    pkt->src_port = 12000;
    pkt->dst_port = 12000;
    pkt->len = 0;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_REQUEST;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0034, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_src_cid_any,
              "CONNECT with src_cid = VMADDR_CID_ANY (0xFFFFFFFF)",
              VIRTIO_SPEC_V1_2, "5.10.6");
