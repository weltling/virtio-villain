/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0023: Console transmit with all descriptor flags set.
 *
 * Submit a transmitq descriptor with NEXT, WRITE, and INDIRECT
 * flags simultaneously. Spec 2.7.5.3.1 says NEXT and INDIRECT are
 * mutually exclusive. The device must reject this combination.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_all_flags(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE |
                       VRING_DESC_F_INDIRECT, 1);
    vring_raw_set_desc(vr, 1, buf_phys + 16, 16, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0023, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_all_flags,
                "Console transmit with NEXT|WRITE|INDIRECT flags",
                VIRTIO_SPEC_V1_2, "2.7.5.3.1", 1);
