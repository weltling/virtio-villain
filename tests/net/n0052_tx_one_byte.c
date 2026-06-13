/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0052: net_tx_minimum_frame
 *
 * Transmit a packet with exactly 1 byte of payload (after the
 * virtio_net_hdr). This is below minimum Ethernet frame size.
 * The device must either pad or reject, but not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_one_byte(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* TX on queue 1 */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    payload[0] = 0x42; /* 1 byte of frame data */

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(payload), 1, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0052, VIRTIO_PCI_DEVICE_NET, test_net_tx_one_byte,
              "TX with exactly 1 byte of payload (below min frame)",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
