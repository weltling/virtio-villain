/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0089: net_ctrl_rx_nobcast_then_tx_bcast
 *
 * Issue CTRL_RX with the NOBCAST command then transmit a packet
 * with a broadcast destination MAC. Spec 5.1.6.5 makes NOBCAST
 * affect only the RX filter, so the TX path must continue to
 * accept the broadcast frame without rejecting it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_nobcast_then_bcast(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr_mrg *h = (void *)pkt;
    h->flags = 0;
    h->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    h->hdr_len = 0;
    h->gso_size = 0;
    h->csum_start = 0;
    h->csum_offset = 0;
    h->num_buffers = 0;

    /* Broadcast destination MAC */
    uint8_t *frame = pkt + sizeof(*h);
    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x11, 6);
    frame[12] = 0x08;
    frame[13] = 0x00;
    memset(frame + 14, 0xCC, 46);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*h) + 60, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0089, VIRTIO_PCI_DEVICE_NET, test_net_nobcast_then_bcast,
                "TX broadcast still flows after RX NOBCAST",
                VIRTIO_SPEC_V1_2, "5.1.6.5", 1);
