/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0185: tx_addr_self_vring
 *
 * A transmit descriptor points at the queue's own descriptor table.
 * The device must read those bytes as opaque frame data rather than
 * confuse them with descriptor state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_addr_self_vring(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    vring_raw_set_desc(vr, 0, vr->desc_phys, 64, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0185, VIRTIO_PCI_DEVICE_NET, test_net_tx_addr_self_vring,
                "Transmit descriptor addr pointing at the vring",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
