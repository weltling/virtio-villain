/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0092: device_feature_select out of range read returns zero.
 *
 * Spec 4.1.4.3: device_feature_select selects which 32 bit
 * window of device_feature to expose. Values beyond what the
 * device implements must return all zeros (no garbage).
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0xFFFE;
    __sync_synchronize();
    uint32_t f = cfg->device_feature;
    if (f != 0)
        TFAIL("device_feature with oob select returned 0x%x", f);
    return TEST_PASS;
}

REGISTER_TEST(PCI0092, VIRTIO_PCI_DEVICE_BLK, test,
              "device_feature_select OOB read returns zero",
              VIRTIO_SPEC_V1_4, "4.1.4.3");
