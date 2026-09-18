/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0201: tx_qinq_double_tag
 *
 * Transmit an Ethernet frame carrying two stacked 802.1Q tags, an
 * outer service tag (TPID 0x88a8) and an inner customer tag (TPID
 * 0x8100), before the real EtherType. This is a valid QinQ frame on
 * the wire. The transmit path parses the header to find the L3 offset
 * for offload and filtering, and a device that assumes a single tag
 * may misread the EtherType or walk past the frame. The device must
 * transmit or drop the frame without wedging the queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_qinq_double_tag(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_net_hdr_mrg *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;
    hdr->num_buffers = 0;

    uint8_t *p = frame;
    memset(p, 0xFF, 6); p += 6;          /* dst MAC broadcast */
    memset(p, 0x02, 6); p += 6;          /* src MAC */
    *p++ = 0x88; *p++ = 0xA8;            /* outer TPID (S-tag, 802.1ad) */
    *p++ = 0x00; *p++ = 0x64;            /* outer TCI, VLAN 100 */
    *p++ = 0x81; *p++ = 0x00;            /* inner TPID (C-tag, 802.1Q) */
    *p++ = 0x00; *p++ = 0xC8;            /* inner TCI, VLAN 200 */
    *p++ = 0x08; *p++ = 0x00;            /* EtherType IPv4 */
    memset(p, 0, 46); p += 46;           /* payload to reach min frame */

    uint32_t frame_len = (uint32_t)(p - frame);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, frame_len, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0201, VIRTIO_PCI_DEVICE_NET, test_net_tx_qinq_double_tag,
              "TX with stacked QinQ double VLAN tags",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
