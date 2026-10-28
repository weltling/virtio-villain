/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0351: sparse_q02_msix
 *
 * Spec 4.1.5.1.2: a used buffer notification for a virtqueue goes to
 * the MSI-X vector bound to that virtqueue. Bind queue 0, 1 and 2 to
 * vectors 0, 1 and 2, enable queues 0 and 2 and leave queue 1
 * disabled. All entries stay masked so notifications land in the
 * Pending Bit Array. A read on queue 2 must set PBA bit 2 only. A
 * device that indexes its workers by position among the enabled queues
 * treats queue 2 as queue 1 and sets bit 1. B0350 is the dense control.
 */
#include "tests/test.h"

static test_result_t test_blk_sparse_q02_msix(struct virtio_dev *dev,
                                              struct vring *unused)
{
    (void)dev;
    (void)unused;
    TSKIP("not implemented");
}

REGISTER_TEST_REQUIRES(B0351, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_sparse_q02_msix,
                       "Queues 0 and 2 enabled: queue 2 signals its own vector",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_MQ), 3);
