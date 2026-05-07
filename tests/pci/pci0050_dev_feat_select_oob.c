/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0050: Read device_feature_select beyond 1.
 *
 * Spec 4.1.4.3: device_feature_select selects which 32-bit slice
 * of feature bits is exposed. Selecting values >1 yields
 * undefined slices; the device must read back 0 for any slice not
 * defined and not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_dev_feat_select_oob(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;
    for (uint32_t sel = 2; sel < 8; sel++) {
        dev->common->device_feature_select = sel;
        __sync_synchronize();
        (void)dev->common->device_feature;
    }
    dev->common->device_feature_select = 0;
    return TEST_PASS;
}

REGISTER_TEST(PCI0050, 0, test_pci_dev_feat_select_oob,
              "device_feature_select beyond defined slices",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
