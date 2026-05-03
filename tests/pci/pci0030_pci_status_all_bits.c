/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0030: PCI device_status write with all bits set (spec 4.1.4.3.1)
 *
 * Write 0xFF to device_status (all defined + undefined bits).
 * FAILED(128) + NEEDS_RESET(64) + various other bits. The device
 * must handle this without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_status_all_bits(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Write 0xFF - sets FAILED, NEEDS_RESET, undefined bits */
    cfg->device_status = 0xFF;
    __sync_synchronize();
    usleep(10000);

    /* Read back - device may mask undefined bits */
    uint8_t status = cfg->device_status;
    (void)status;

    /* Reset to recover */
    cfg->device_status = 0;
    __sync_synchronize();

    int tries = 100;
    while (tries-- > 0 && cfg->device_status != 0)
        usleep(1000);

    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    /* Re-init to verify recovery */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    usleep(5000);

    status = cfg->device_status;
    if (!(status & VIRTIO_STATUS_ACKNOWLEDGE))
        TWEDGED("!(status & VIRTIO_STATUS_ACKNOWLEDGE)");

    TREJECT("!(status & VIRTIO_STATUS_ACKNOWLEDGE)");
}

REGISTER_TEST(PCI0030, VIRTIO_PCI_DEVICE_BLK, test_pci_status_all_bits,
              "Write 0xFF to device_status (all bits set)",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
