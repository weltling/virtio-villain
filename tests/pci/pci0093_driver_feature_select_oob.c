/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0093: driver_feature_select out of range write ignored.
 *
 * Spec 4.1.4.3: driver_feature_select selects which 32 bit
 * window of driver_feature the next write addresses. An OOB
 * value must not modify accepted features. Set OOB then write
 * 0xFFFFFFFF; reset select and verify low and high windows
 * remain zero.
 */
#include "tests/test.h"

#include <unistd.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0xFFFE;
    cfg->driver_feature = 0xFFFFFFFFu;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    __sync_synchronize();
    if (cfg->driver_feature != 0)
        TFAIL("driver_feature lo non zero after OOB select write");
    cfg->driver_feature_select = 1;
    __sync_synchronize();
    if (cfg->driver_feature != 0)
        TFAIL("driver_feature hi non zero after OOB select write");

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(1000);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    return TEST_PASS;
}

REGISTER_TEST(PCI0093, VIRTIO_PCI_DEVICE_BLK, test,
              "driver_feature_select OOB write does not leak",
              VIRTIO_SPEC_V1_4, "4.1.4.3");
