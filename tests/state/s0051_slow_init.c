/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0051: long pauses between every init step
 *
 * Spec 2.1.2 lays out the init sequence. Real drivers do not
 * always race through it. Insert deliberate delays between every
 * status transition and feature step to catch devices that time
 * out internal state machines or assume the driver moves quickly.
 */
#include "lib/virtio_test.h"

static test_result_t test_slow_init(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    virtio_pci_reset(dev);
    usleep(100000);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    usleep(100000);

    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    usleep(100000);

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = virtio_pci_base_features_hi(dev);
    __sync_synchronize();
    usleep(100000);

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(100000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    return TEST_PASS;
}

REGISTER_TEST_DEV(S0051, VIRTIO_PCI_DEVICE_BLK, test_slow_init,
                  "100ms pause between every init step",
                  VIRTIO_SPEC_V1_2, "2.1.2");
