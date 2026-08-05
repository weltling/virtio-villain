/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0194: rx_max_chain
 *
 * A receive buffer spans a full ring of writable descriptors. The
 * device must accept the long chain without overrunning the ring.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define CHAIN 16

static test_result_t test_net_rx_max_chain(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    for (int i = 0; i < CHAIN; i++) {
        uint16_t last = i == CHAIN - 1;
        vring_raw_set_desc(vr, i, vv_virt_to_phys(buf + (i * 128 % 4096)), 128,
                           VRING_DESC_F_WRITE | (last ? 0 : VRING_DESC_F_NEXT),
                           last ? 0 : i + 1);
    }
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0194, VIRTIO_PCI_DEVICE_NET, test_net_rx_max_chain,
                "Receive buffer spanning a full ring of descriptors",
                VIRTIO_SPEC_V1_2, "2.7.5", 0);
