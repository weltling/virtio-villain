/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0079: driver_ok_set_twice
 *
 * Set DRIVER_OK, then set it again without an intervening reset.
 * Spec 3.1.1 says DRIVER_OK is set once to signal the driver is
 * ready. Setting it twice should be idempotent; the device must
 * not treat the second write as a restart or crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_driver_ok_twice(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Already in DRIVER_OK from init; write it again */
    uint8_t old = cfg->device_status;
    if (!(old & VIRTIO_STATUS_DRIVER_OK))
        return TEST_SKIP;

    cfg->device_status = old;
    __sync_synchronize();
    usleep(50000);

    /* Write DRIVER_OK explicitly again */
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(50000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK)");
    return TEST_PASS;
}

REGISTER_TEST(S0079, VIRTIO_PCI_DEVICE_BLK, test_driver_ok_twice,
              "Set DRIVER_OK twice without reset",
              VIRTIO_SPEC_V1_2, "3.1.1");
