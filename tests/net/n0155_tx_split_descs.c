/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0155: TX frame split across two descriptors.
 *
 * Submit a valid frame where the virtio_net_hdr is in descriptor 0
 * and the Ethernet data is in descriptor 1 (chained via NEXT).
 * This is the standard two-descriptor TX layout.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_split(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0; hdr->gso_size = 0;
    hdr->csum_start = 0; hdr->csum_offset = 0;

    memset(frame, 0xFF, 6); memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0xAB, 46);

    /* Two descriptors: hdr -> frame */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 60, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0155, VIRTIO_PCI_DEVICE_NET, test_net_tx_split,
              "TX frame split across two descriptors",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
