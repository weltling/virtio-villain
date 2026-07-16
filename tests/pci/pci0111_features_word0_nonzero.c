/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0111: device feature word 0 is non zero.
 *
 * Spec 4.1.4.3: Every virtio device must offer at least some
 * features in word 0 (VERSION_1 is in word 1, but device type
 * features are in word 0). Read device_feature with select=0
 * and verify non zero.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_feat_nonzero(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0 = cfg->device_feature;

    if (f0 == 0)
        TFAIL("device feature word 0 is 0 (no device features offered)");

    return TEST_PASS;
}

REGISTER_TEST(PCI0111, VIRTIO_PCI_DEVICE_BLK, test_pci_feat_nonzero,
              "Device feature word 0 is non zero",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
