/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0059: TX with broadcast MAC destination (spec 5.1.6.2)
 *
 * Transmit a frame with destination MAC FF:FF:FF:FF:FF:FF.
 * This is a valid broadcast frame. The device should process
 * it normally (not filter/drop it).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_broadcast(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;

    /* Minimal Ethernet frame: dst(6) + src(6) + type(2) + payload(46) = 60 */
    memset(frame, 0xFF, 6);             /* broadcast dest */
    memset(frame + 6, 0x02, 6);         /* src MAC */
    frame[12] = 0x08; frame[13] = 0x00; /* IPv4 ethertype */
    memset(frame + 14, 0x00, 46);       /* padding */

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(frame), 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0059, VIRTIO_PCI_DEVICE_NET, test_net_tx_broadcast,
              "TX frame with broadcast destination MAC",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
