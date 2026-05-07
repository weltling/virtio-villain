/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0038: device_feature_select past supported pages
 *
 * Spec 4.1.4.3.1 says reading device_feature returns the value of
 * the device feature bits selected by device_feature_select. The
 * spec does not bound the select value, so reading with a select
 * far past page 1 must not crash the device or return uninitialized
 * memory. Most devices return 0 for unknown pages.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_feature_select_oob(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    for (uint32_t sel = 2; sel < 16; sel++) {
        cfg->device_feature_select = sel;
        __sync_synchronize();
        uint32_t v = cfg->device_feature;
        /* Any value is acceptable, but device must remain alive */
        (void)v;
    }

    /* Device must still be alive afterwards */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TFAIL("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0038, VIRTIO_PCI_DEVICE_BLK, test_feature_select_oob,
              "device_feature_select past page 1 keeps device alive",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
