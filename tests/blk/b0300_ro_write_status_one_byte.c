/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0300: ro_write_status_one_byte
 *
 * A write to a read only device must complete with VIRTIO_BLK_S_IOERR
 * (spec 5.2.6.2). The status field is a single byte, so the device must
 * not write past the 1 byte status descriptor. The sidecar launches the
 * VM with a read only disk.
 */
#include "tests/test.h"

static test_result_t test_blk_ro_write_status_one_byte(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    (void)dev;
    (void)vr;
    TSKIP("not implemented");
}

REGISTER_TEST_REQUIRES(B0300, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_ro_write_status_one_byte,
                       "Read-only write: status is exactly one byte",
                       VIRTIO_SPEC_V1_2, "5.2.6.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_RO), 0);
