/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0040: PERSISTENT_SUSPEND feature advertisement.
 *
 * v1.4 5.15.3: VIRTIO_MEM_F_PERSISTENT_SUSPEND (bit 3) when
 * negotiated lets the device preserve plugged state across a
 * suspend/resume cycle. The orchestrator drives the actual
 * suspend; this guest side test verifies the feature can be
 * negotiated (or skip).
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_persistent_suspend(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_MEM_F_PERSISTENT_SUSPEND))
        return TEST_SKIP;
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(R0040, VIRTIO_PCI_DEVICE_MEM, test_mem_persistent_suspend,
              "PERSISTENT_SUSPEND feature offered",
              VIRTIO_SPEC_V1_4, "5.15.3",
              (1ULL << VIRTIO_MEM_F_PERSISTENT_SUSPEND), 0);
