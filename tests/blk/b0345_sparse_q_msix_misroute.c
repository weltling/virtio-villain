/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0345: sparse_q_msix_misroute
 *
 * Spec 4.1.5.1.2: the device signals a used buffer notification for a
 * virtqueue on the MSI-X vector the driver bound to that virtqueue via
 * queue_msix_vector. A driver may enable only a subset of the queues,
 * for example queue 1 without queue 0.
 *
 * Bind queue 0 to vector 0 but leave it disabled, bind queue 1 to
 * vector 1 and enable it. Both MSI-X entries stay masked, so the
 * device records the notification in the Pending Bit Array instead of
 * delivering it, which needs no guest IRQ infrastructure. A read on
 * queue 1 must set PBA bit 1 and must not set bit 0. A device that
 * numbers its queue workers by their position among the enabled queues
 * instead of by the real queue index signals queue 0's vector.
 */
#include "tests/test.h"

static test_result_t test_blk_sparse_q_msix_misroute(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    (void)dev;
    (void)vr;
    TSKIP("not implemented");
}

REGISTER_TEST_REQUIRES(B0345, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_sparse_q_msix_misroute,
                       "Queue 1 alone enabled signals queue 1's MSI-X vector",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_MQ), 2);
