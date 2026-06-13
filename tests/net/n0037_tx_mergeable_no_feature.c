/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0037: net_tx_mergeable_no_feature
 *
 * Submit a TX packet using the 12-byte mergeable RX header format
 * (with num_buffers) without negotiating VIRTIO_NET_F_MRG_RXBUF.
 * The device expects the 10-byte header without MRG_RXBUF. Using
 * the larger header shifts the frame data by 2 bytes, potentially
 * causing the device to misparse the ethernet frame.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_mrg_no_feature(struct virtio_dev *dev,
                                                struct vring *vr)
{
    /*
     * Send using 12-byte header (mergeable format).
     * Without MRG_RXBUF negotiated, device expects 10 bytes.
     */
    struct virtio_net_hdr_mrg *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;
    hdr->num_buffers = 3; /* meaningless on TX, but pollutes header */

    /* Frame data immediately follows the 12-byte header */
    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0x41, 46);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t frame_phys = vv_virt_to_phys(frame);

    /* Use 12-byte header size (mergeable) in descriptor */
    vring_raw_set_desc(vr, 0, hdr_phys, 12, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, frame_phys, 60, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0037, VIRTIO_PCI_DEVICE_NET, test_net_tx_mrg_no_feature,
              "TX with 12-byte mergeable header without MRG_RXBUF",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
