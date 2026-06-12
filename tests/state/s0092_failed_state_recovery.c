/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0092: recovery from FAILED via reset and reinit.
 *
 * Spec 2.1.1: Setting FAILED is a terminal state; the only
 * way out is a device reset followed by reinit. Set FAILED,
 * then reset and run the standard init sequence; the second
 * round must reach DRIVER_OK.
 */
#include "tests/test.h"

#include <unistd.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_status |= VIRTIO_STATUS_FAILED;
    __sync_synchronize();
    usleep(1000);

    virtio_pci_reset(dev);
    if (virtio_pci_init(dev) < 0)
        TFAIL("reinit after FAILED failed");
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("DRIVER_OK not set after recovery");
    return TEST_PASS;
}

REGISTER_TEST(S0092, VIRTIO_PCI_DEVICE_BLK, test,
              "Recovery from FAILED via reset",
              VIRTIO_SPEC_V1_4, "2.1.1");
