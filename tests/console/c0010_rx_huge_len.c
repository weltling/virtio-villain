/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0010: Console receive with oversized writable length.
 *
 * Submit a receiveq descriptor whose length field claims a 1 GiB
 * writable buffer while the underlying allocation is only one page.
 * The device must validate the length against guest memory bounds
 * and must not write past the end of the real buffer when host
 * input arrives.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_rx_huge_len(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint64_t buf_phys;
    vv_alloc_page_high(&buf_phys);

    vring_raw_set_desc(vr, 0, buf_phys, 1u << 30, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(C0010, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_huge_len,
              "Console receive with 1 GiB writable length",
              VIRTIO_SPEC_V1_2, "5.3");
