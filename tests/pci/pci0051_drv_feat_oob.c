/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0051: Write driver_feature outside valid select.
 *
 * Set driver_feature_select to a value >1 and write nonzero bits
 * to driver_feature. The VMM must ignore writes to undefined
 * slices.
 *
 * Spec 4.1.4.3.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_drv_feat_oob(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    dev->common->driver_feature_select = 5;
    __sync_synchronize();
    dev->common->driver_feature = 0xDEADBEEF;
    __sync_synchronize();
    dev->common->driver_feature_select = 0;
    return TEST_PASS;
}

REGISTER_TEST(PCI0051, 0, test_pci_drv_feat_oob,
              "driver_feature write to undefined slice",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
