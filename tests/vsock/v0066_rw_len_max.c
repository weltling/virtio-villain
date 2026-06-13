/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0066: vsock OP_RW with packet len = 0xFFFFFFFF.
 *
 * Spec 5.10.6.6: Each vsock packet has a 32 bit len field that
 * the device uses to track the payload size. Submit OP_RW with
 * len set to 0xFFFFFFFF while only providing a tiny payload
 * descriptor. The device must reject the header that claims
 * more payload than the chain actually supplies rather than
 * trusting len and reading past the buffer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vsock_rw_len_max(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_vsock_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    hdr->src_cid   = 3;
    hdr->dst_cid   = 2;
    hdr->src_port  = 0xC0DE;
    hdr->dst_port  = 0xBA5E;
    hdr->type      = VIRTIO_VSOCK_TYPE_STREAM;
    hdr->op        = VIRTIO_VSOCK_OP_RW;
    hdr->len       = 0xFFFFFFFFU;
    hdr->buf_alloc = 0x10000;

    memset(payload, 'A', 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 64, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0066, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_rw_len_max,
              "OP_RW packet len = 0xFFFFFFFF with tiny payload",
              VIRTIO_SPEC_V1_2, "5.10.6.6");
