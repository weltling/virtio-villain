/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0101: device offers VIRTIO_F_VERSION_1.
 *
 * Spec 6: A device MUST offer VIRTIO_F_VERSION_1. Read feature
 * bits word 1 and verify bit 0 (VERSION_1 at global bit 32) is set.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_version_1(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t hi = cfg->device_feature;

    if (!(hi & (1u << (VIRTIO_F_VERSION_1 - 32))))
        TFAIL("VIRTIO_F_VERSION_1 not offered");

    return TEST_PASS;
}

REGISTER_TEST(PCI0101, VIRTIO_PCI_DEVICE_BLK, test_pci_version_1,
              "Device offers VIRTIO_F_VERSION_1",
              VIRTIO_SPEC_V1_2, "6");
