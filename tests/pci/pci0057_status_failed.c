/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0057: device_status set FAILED bit.
 *
 * Spec 2.1: when the driver sets FAILED (0x80), the device must
 * remain accessible for reset. Set FAILED, read back status, then
 * reset.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_status_failed(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    dev->common->device_status |= VIRTIO_STATUS_FAILED;
    __sync_synchronize();
    usleep(5000);
    uint8_t s = dev->common->device_status;
    if (!(s & VIRTIO_STATUS_FAILED))
        TFAIL("!(s & VIRTIO_STATUS_FAILED)");
    dev->common->device_status = 0;
    __sync_synchronize();
    return TEST_PASS;
}

REGISTER_TEST(PCI0057, 0, test_pci_status_failed,
              "device_status FAILED bit",
              VIRTIO_SPEC_V1_2, "2.1");
