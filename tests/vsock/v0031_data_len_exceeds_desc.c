/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0031: vsock_data_len_exceeds_desc
 *
 * Send a DATA packet where the header's len field claims 0xFFFFFFFF
 * bytes of payload but the actual descriptor is only 64 bytes
 * (header size). Tests device handling of len/descriptor mismatch.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_data_len_huge(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 9000;
    pkt->dst_port = 9000;
    pkt->len = 0xFFFFFFFF; /* claims 4 GiB of payload */
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0;
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    /* Descriptor only covers the header - no actual payload */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0031, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_data_len_huge,
              "DATA with len=0xFFFFFFFF but descriptor only covers header",
              VIRTIO_SPEC_V1_2, "5.10.6");
