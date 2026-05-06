/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0020: Console transmit indirect entry with INDIRECT flag.
 *
 * Submit an indirect transmitq descriptor whose first entry itself
 * has VRING_DESC_F_INDIRECT set, attempting to nest indirect
 * tables. Spec 2.7.7 forbids nesting. The device must reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_indirect_self(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct vring_desc *itab = vv_alloc_pages(1);
    uint64_t itab_phys = vv_virt_to_phys(itab);

    itab[0].addr  = itab_phys;
    itab[0].len   = sizeof(*itab);
    itab[0].flags = VRING_DESC_F_INDIRECT;
    itab[0].next  = 0;

    vring_raw_set_desc(vr, 0, itab_phys, sizeof(*itab),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0020, VIRTIO_PCI_DEVICE_CONSOLE,
                test_console_tx_indirect_self,
                "Console transmit nested indirect",
                VIRTIO_SPEC_V1_2, "2.7.7", 1);
