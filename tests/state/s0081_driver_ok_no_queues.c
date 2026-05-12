/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0081: driver_ok_without_queues
 *
 * Reset the device, walk through ACKNOWLEDGE, DRIVER, feature
 * negotiation, and FEATURES_OK, then set DRIVER_OK without
 * enabling any queue. Spec 3.1.1 step 7 says the driver must
 * perform device specific setup including virtqueue configuration
 * before step 8 (DRIVER_OK). The device must not crash when
 * activation finds zero ready queues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_driver_ok_no_queues(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);

    /* Steps 1-2: ACKNOWLEDGE, DRIVER */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Steps 4-5: accept whatever features, set FEATURES_OK */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    /* Skip step 7 (queue setup) entirely */

    /* Step 8: DRIVER_OK without any queue enabled */
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(100000);

    /* Device must still be alive */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0081, VIRTIO_PCI_DEVICE_BLK, test_driver_ok_no_queues,
              "DRIVER_OK without enabling any queue",
              VIRTIO_SPEC_V1_2, "3.1.1");
