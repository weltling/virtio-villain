/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0006: Console transmit with NEXT pointing past the ring.
 *
 * Build a TX chain whose head sets VRING_DESC_F_NEXT and references
 * a descriptor index outside the ring (0xFFFE). The device must
 * reject the chain rather than dereference an out-of-bounds slot.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_oob_next(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 1, VRING_DESC_F_NEXT, 0xFFFE);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0006, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_oob_next,
                "Console transmit NEXT pointing past ring",
                VIRTIO_SPEC_V1_2, "5.3", 1);
