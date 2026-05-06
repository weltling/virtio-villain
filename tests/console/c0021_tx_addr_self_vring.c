/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0021: Console transmit pointing at the descriptor table.
 *
 * Submit a transmitq descriptor whose addr points at the queue's
 * own descriptor table. The device must read the bytes as opaque
 * data without confusing them with descriptor state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_addr_self_vring(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    /* Point the descriptor's addr at the desc table itself, length 16. */
    vring_raw_set_desc(vr, 0, vr->desc_phys, 16, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0021, VIRTIO_PCI_DEVICE_CONSOLE,
                test_console_tx_addr_self_vring,
                "Console transmit addr pointing at vring",
                VIRTIO_SPEC_V1_2, "5.3", 1);
