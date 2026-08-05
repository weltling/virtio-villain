/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0188: tx_max_chain
 *
 * A transmit request spans a full ring worth of descriptors, the
 * header followed by many small readable fragments. The device must
 * gather the whole chain without overrunning the ring.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define CHAIN 16

static test_result_t test_net_tx_max_chain(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frag = vv_alloc_pages(1);
    memset(hdr, 0, sizeof(*hdr));
    memset(frag, 0x5a, 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    for (int i = 1; i < CHAIN; i++) {
        uint16_t last = i == CHAIN - 1;
        vring_raw_set_desc(vr, i, vv_virt_to_phys(frag + (i * 64 % 4096)), 64,
                           last ? 0 : VRING_DESC_F_NEXT,
                           last ? 0 : i + 1);
    }
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0188, VIRTIO_PCI_DEVICE_NET, test_net_tx_max_chain,
                "Transmit request spanning a full ring of descriptors",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
