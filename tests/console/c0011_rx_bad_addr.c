/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0011: Console receive with out-of-range address.
 *
 * Submit a writable receiveq descriptor whose addr field points
 * outside any valid guest memory region. The device must fail any
 * write attempt safely rather than pass an unchecked address into
 * the host's translation path.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_rx_bad_addr(struct virtio_dev *dev,
                                              struct vring *vr)
{
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL, 64,
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(C0011, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_bad_addr,
              "Console receive with out-of-range address",
              VIRTIO_SPEC_V1_2, "5.3");
