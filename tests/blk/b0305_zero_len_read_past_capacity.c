/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0305: zero_len_read_past_capacity
 *
 * Spec 5.2.6.1: a driver MUST NOT submit a request which would cause a
 * read beyond capacity. A READ whose data descriptor has len 0
 * transfers nothing, so a device may skip the capacity check, but it
 * must then treat every sector value the same way. QEMU rejects
 * sector > capacity regardless of the length.
 *
 * Send two zero length READs, at sector capacity + 8 and at sector
 * 1 << 54 (the byte offset 1 << 63 is negative as off_t). The test
 * fails if the two get different status codes, or if either is S_OK.
 */
#include "tests/test.h"

static test_result_t test_blk_zero_len_read_past_cap(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    (void)dev;
    (void)vr;
    TSKIP("not implemented");
}

REGISTER_TEST(B0305, VIRTIO_PCI_DEVICE_BLK, test_blk_zero_len_read_past_cap,
              "Zero-length READ past capacity rejected consistently",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
