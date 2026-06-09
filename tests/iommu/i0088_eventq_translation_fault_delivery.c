/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0088: event queue is configured when PROBE feature offered.
 *
 * v1.4 5.13.5: the IOMMU device may offer an event virtqueue
 * (idx 1) for fault delivery. Verify num_queues >= 2 when
 * the device offers PROBE (translation faults relevant only
 * when full topology probing is exposed).
 */
#include "tests/test.h"
#include "lib/virtio_iommu.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_IOMMU_F_PROBE)))
        return TEST_SKIP;
    if (cfg->num_queues < 2)
        TFAIL("PROBE offered but no event queue (num_queues=%u)",
              cfg->num_queues);
    return TEST_PASS;
}

REGISTER_TEST(I0088, VIRTIO_PCI_DEVICE_IOMMU, test,
              "Event queue exposed when PROBE offered",
              VIRTIO_SPEC_V1_4, "5.13.5");
