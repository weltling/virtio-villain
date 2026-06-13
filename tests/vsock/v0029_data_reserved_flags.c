/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0029: vsock_data_reserved_flags
 *
 * Send a DATA packet with reserved (undefined) bits set in the flags
 * field. The device must either ignore unknown flags or reject the
 * packet, but must not interpret them as valid operations.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_data_reserved_flags(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);
    uint8_t *payload = (uint8_t *)pkt + sizeof(*pkt);

    pkt->src_cid = 3;
    pkt->dst_cid = 2;
    pkt->src_port = 8888;
    pkt->dst_port = 8888;
    pkt->len = 4;
    pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    pkt->op = VIRTIO_VSOCK_OP_RW;
    pkt->flags = 0xFFFFFFFC; /* all reserved bits set, valid bits (0,1) clear */
    pkt->buf_alloc = 4096;
    pkt->fwd_cnt = 0;

    memcpy(payload, "TEST", 4);

    uint64_t pkt_phys = vv_virt_to_phys(pkt);

    vring_raw_set_desc(vr, 0, pkt_phys, sizeof(*pkt) + 4, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0029, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_data_reserved_flags,
              "DATA packet with reserved flag bits set",
              VIRTIO_SPEC_V1_2, "5.10.6");
