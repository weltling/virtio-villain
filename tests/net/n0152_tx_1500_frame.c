/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0152: TX 1500 byte frame (typical MTU size).
 *
 * Submit a full MTU sized Ethernet frame (14 byte header + 1486
 * byte payload = 1500 total). Tests that the device handles
 * standard MTU size frames.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_1500(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0; hdr->gso_size = 0;
    hdr->csum_start = 0; hdr->csum_offset = 0;

    /* 1500 byte Ethernet frame */
    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0x42, 1486);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 1500, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0152, VIRTIO_PCI_DEVICE_NET, test_net_tx_1500,
              "TX 1500 byte frame (standard MTU)",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
