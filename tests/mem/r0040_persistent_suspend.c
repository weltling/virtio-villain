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
#include "tests/test.h"
#include "lib/virtio_spec.h"

static test_result_t test_mem_persistent_suspend(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_MEM_F_PERSISTENT_SUSPEND)))
        return TEST_SKIP;
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(R0040, VIRTIO_PCI_DEVICE_MEM, test_mem_persistent_suspend,
              "PERSISTENT_SUSPEND feature offered",
              VIRTIO_SPEC_V1_4, "5.15.3",
              (1ULL << VIRTIO_MEM_F_PERSISTENT_SUSPEND), 0);
