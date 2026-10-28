/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0350: dense_q_msix_control
 *
 * Control for B0345, B0346 and B0351. Same Pending Bit Array based
 * check, but queue 0 is enabled too (vector 0) so the enabled queues
 * are dense. A read on queue 1 must set PBA bit 1 and not bit 0. This
 * passing while B0345 fails isolates the bug to sparse queue
 * activation.
 */
#include "tests/test.h"

static test_result_t test_blk_dense_q_msix_control(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)dev;
    (void)vr;
    TSKIP("not implemented");
}

REGISTER_TEST_REQUIRES(B0350, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_dense_q_msix_control,
                       "Dense queues: queue 1 signals MSI-X vector 1 (control)",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_MQ), 2);
