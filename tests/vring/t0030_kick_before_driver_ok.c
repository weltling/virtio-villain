/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0030: kick_before_driver_ok
 *
 * Reset the device back to ACKNOWLEDGE status (without DRIVER_OK) and
 * then send a queue notification. A VMM that does not guard against
 * notifications on a not-yet-active queue may dereference uninitialized
 * queue state, panic, or corrupt memory.
 *
 * Correct behavior: ignore the kick entirely. The spec says the device
 * "MUST NOT consume buffers or send any used buffer notifications to
 * the driver before DRIVER_OK."
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_kick_before_driver_ok(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;

    /*
     * The harness already set DRIVER_OK. Reset the device and bring it
     * only to ACKNOWLEDGE + DRIVER (no queue setup, no DRIVER_OK).
     */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /*
     * Kick queue 0 without ever setting up descriptors, without
     * FEATURES_OK, and without DRIVER_OK. This is a blatant protocol
     * violation.
     */
    virtio_pci_kick(dev, 0);

    /* Kick again a few times to increase the chance of racing the
     * VMM's internal state machine. */
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 0);

    /* Give the VMM time to crash or panic */
    usleep(200000);

    /*
     * If we're still alive, check whether the device is still
     * responsive by completing the init sequence.
     */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("device did not accept FEATURES_OK after kick-before-ready");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    /* Device survived - it correctly ignored the premature kick */
    return TEST_PASS;
}

REGISTER_TEST(T0030, VIRTIO_PCI_DEVICE_BLK, test_kick_before_driver_ok,
              "Queue kick before DRIVER_OK",
              VIRTIO_SPEC_V1_2, "3.1");
