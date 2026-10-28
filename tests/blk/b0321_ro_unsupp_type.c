/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0321: ro_unsupp_type
 *
 * VIRTIO_BLK_F_RO only changes the result of write requests to IOERR
 * (spec 5.2.6.2). A request type the device does not implement at all
 * (GET_LIFETIME without VIRTIO_BLK_F_LIFETIME, an unassigned type) is
 * still unsupported and should return VIRTIO_BLK_S_UNSUPP, so a driver
 * can tell "not implemented" from "media error". The sidecar launches
 * the VM with a read only disk.
 */
#include "tests/test.h"

static test_result_t test_blk_ro_unsupp_type(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)dev;
    (void)vr;
    TSKIP("not implemented");
}

REGISTER_TEST_REQUIRES(B0321, VIRTIO_PCI_DEVICE_BLK, test_blk_ro_unsupp_type,
                       "Read-only device: unsupported types return UNSUPP",
                       VIRTIO_SPEC_V1_2, "5.2.6.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_RO), 0);
