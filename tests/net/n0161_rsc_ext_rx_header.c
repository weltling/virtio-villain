/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0161: net_rsc_ext_rx_header
 *
 * Post a receive buffer with VIRTIO_NET_F_RSC_EXT negotiated and
 * check that any coalescing report the device writes is well formed.
 * Spec 5.1.6.4: when RSC_EXT is negotiated and the device coalesces
 * received segments it sets VIRTIO_NET_HDR_F_RSC_INFO in flags and
 * stores the coalesced packet count in csum_start and the duplicate
 * ACK count in csum_offset. A report that sets the flag but claims
 * zero coalesced packets is invalid.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rsc_ext_rx_header(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    /* Post a writable receive buffer large enough for a mergeable
     * header plus a full frame. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf),
                       sizeof(struct virtio_net_hdr_mrg) + 1500,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* The device completed the buffer. If it set the RSC info flag
     * the coalesced-packet count must be non-zero. */
    volatile struct virtio_net_hdr_mrg *hdr =
        (volatile struct virtio_net_hdr_mrg *)buf;
    if (hdr->flags & VIRTIO_NET_HDR_F_RSC_INFO) {
        uint16_t coalesced = hdr->csum_start;
        if (coalesced == 0)
            TFAIL("RSC_INFO set but coalesced packet count is 0");
    }

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0161, VIRTIO_PCI_DEVICE_NET, test_net_rsc_ext_rx_header,
                       "RSC_EXT receive coalescing report is well formed",
                       VIRTIO_SPEC_V1_4, "5.1.6.4",
                       (1ULL << VIRTIO_NET_F_RSC_EXT), 0);
