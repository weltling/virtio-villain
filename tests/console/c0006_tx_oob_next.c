/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0006: Console transmit with NEXT pointing past the ring.
 *
 * Build a TX chain whose head sets VRING_DESC_F_NEXT and references
 * a descriptor index outside the ring (0xFFFE). The device must
 * reject the chain rather than dereference an out-of-bounds slot.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_oob_next(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 1, VRING_DESC_F_NEXT, 0xFFFE);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0006, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_oob_next,
                "Console transmit NEXT pointing past ring",
                VIRTIO_SPEC_V1_2, "5.3", 1);
