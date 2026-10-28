/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0346: sparse_q_msix_lost
 *
 * Spec 4.1.5.1.2: the device signals a used buffer notification for a
 * virtqueue on the MSI-X vector bound to that virtqueue. Enable only
 * queue 1 (bound to vector 1) and leave queue 0 disabled with
 * NO_VECTOR, as a driver that does not use queue 0 does. Both MSI-X
 * entries stay masked so the notification lands in the Pending Bit
 * Array. A read on queue 1 must set PBA bit 1. A device that signals
 * the vector of the wrong queue drops the notification, and an
 * interrupt driven driver waits forever.
 */
#include "tests/test.h"

static test_result_t test_blk_sparse_q_msix_lost(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)dev;
    (void)vr;
    TSKIP("not implemented");
}

REGISTER_TEST_REQUIRES(B0346, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_sparse_q_msix_lost,
                       "Sparse queue 1 notification not lost, queue 0 unbound",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_MQ), 2);
