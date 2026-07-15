/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0151: TX with header only, zero payload.
 *
 * Spec 5.1.6.2: The driver prepends virtio_net_hdr. Submit a frame
 * with only the virtio header and zero bytes of Ethernet data. The
 * device should handle the zero length payload gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_hdr_only(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    /* Single descriptor: just the virtio header, no Ethernet frame */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0151, VIRTIO_PCI_DEVICE_NET, test_net_tx_hdr_only,
              "TX with header only and zero payload",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
