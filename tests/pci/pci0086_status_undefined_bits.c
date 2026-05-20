/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0086: Write device_status with undefined bits set.
 *
 * Spec 4.1.4.3.1: The device_status field has defined bits 0..7.
 * Writing with undefined upper bits set is a protocol violation
 * but must not crash the device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_status_undefined_bits(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    (void)vr;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint8_t saved = cfg->device_status;

    /* Set all bits including undefined ones */
    cfg->device_status = 0xFF;
    __sync_synchronize();
    usleep(100 * 1000);

    /* Restore valid status */
    cfg->device_status = saved;
    __sync_synchronize();
    usleep(100 * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0086, VIRTIO_PCI_DEVICE_BLK, test_pci_status_undefined_bits,
              "Write device_status with undefined bits set",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
