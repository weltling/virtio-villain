/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0007: net_gso_without_csum
 *
 * Set gso_type to TCPV4 without having negotiated VIRTIO_NET_F_CSUM.
 * Spec 5.1.6.2.1: driver MUST NOT set gso_type != NONE unless CSUM
 * is negotiated. The device must reject or ignore.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_gso_without_csum(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    /* GSO type set without CSUM feature negotiated */
    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    hdr->hdr_len = 54;
    hdr->gso_size = 1460;
    hdr->csum_start = 34;
    hdr->csum_offset = 16;

    memset(frame, 0x42, 128);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 128, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0007, VIRTIO_PCI_DEVICE_NET, test_net_gso_without_csum,
              "GSO type set without CSUM feature negotiated",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
