/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0085: queue_setup_before_acknowledge
 *
 * After reset, attempt to select and enable a queue before setting
 * ACKNOWLEDGE. Spec 3.1.1 says queue configuration (step 7) comes
 * after the device has been acknowledged (step 2). Touching queue
 * registers before the device is acknowledged is out of sequence.
 * The device must not crash; it may ignore the writes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_queue_before_ack(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);

    /* Touch queue registers before ACKNOWLEDGE */
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_size = 16;
    __sync_synchronize();
    cfg->queue_enable = 1;
    __sync_synchronize();
    usleep(50000);

    /* Reset and do a proper init to verify device survived */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER)");

    return TEST_PASS;
}

REGISTER_TEST(S0085, VIRTIO_PCI_DEVICE_BLK, test_queue_before_ack,
              "Queue setup before ACKNOWLEDGE",
              VIRTIO_SPEC_V1_2, "3.1.1");
