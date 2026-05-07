/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0040: driver_feature write before DRIVER bit
 *
 * Spec 3.1.1 step 3 says the driver sets DRIVER before reading
 * device_feature and writing driver_feature. Writing driver_feature
 * before DRIVER is set is out of order. The device should either
 * ignore the write (revealed by FEATURES_OK rejection later when
 * driver state is correct) or stay alive without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_driver_feature_before_driver(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    virtio_pci_reset(dev);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    /* Out of order write */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0xDEADBEEF;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0xCAFEBABE;
    __sync_synchronize();

    /* Device must still be alive */
    if (cfg->device_status == 0)
        TFAIL("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0040, VIRTIO_PCI_DEVICE_BLK, test_driver_feature_before_driver,
              "Write driver_feature before DRIVER bit keeps device alive",
              VIRTIO_SPEC_V1_2, "3.1.1");
