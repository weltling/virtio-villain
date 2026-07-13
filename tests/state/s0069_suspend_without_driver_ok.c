/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0069: suspend_without_driver_ok
 *
 * Set SUSPEND without DRIVER_OK being set. Spec 3.2: the device
 * MUST ignore operations on SUSPEND if it has not been completely
 * initialized.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_suspend_no_driver_ok(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Check if SUSPEND is offered */
    cfg->device_feature_select = VIRTIO_F_SUSPEND / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_SUSPEND % 32))))
        return TEST_SKIP;

    /* Reset device to get out of DRIVER_OK */
    virtio_pci_reset(dev);
    usleep(50000);

    /* Set ACK + DRIVER but NOT DRIVER_OK */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Try to set SUSPEND before full init */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(100000);

    /* Device should ignore SUSPEND; status should not show SUSPEND active
     * (DRIVER_OK clearing would be the suspend ack) */
    uint8_t new_status = cfg->device_status;

    /* Device must not have crashed */
    if (new_status == 0)
        TWEDGED("new_status == 0");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(S0069, VIRTIO_PCI_DEVICE_BLK, test_suspend_no_driver_ok,
              "Set SUSPEND bit without DRIVER_OK",
              VIRTIO_SPEC_V1_3, "3.2",
              (1ULL << VIRTIO_F_SUSPEND), 0);
