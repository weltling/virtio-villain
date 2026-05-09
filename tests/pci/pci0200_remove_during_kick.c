/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0200: boot virtio-blk function survives a sibling PCI hot
 * remove of a previously hot added function.
 *
 * The sidecar adds a second blk function with id hp0 and then
 * removes it. The guest watches device_status of the boot blk
 * function across the window and asserts it does not raise FAILED
 * or NEEDS_RESET. This catches host code that walks a global PCI
 * device list and corrupts the boot device on neighbour removal.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_remove_during_kick(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        return TEST_SKIP;
    int waited = 0;
    while (waited < 12000) {
        __sync_synchronize();
        uint8_t st = cfg->device_status;
        if (st & VIRTIO_STATUS_FAILED)
            TFAIL("st & VIRTIO_STATUS_FAILED");
        if (st & VIRTIO_STATUS_NEEDS_RESET)
            TFAIL("st & VIRTIO_STATUS_NEEDS_RESET");
        usleep(100 * 1000);
        waited += 100;
    }
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK)");
    return TEST_PASS;
}

REGISTER_TEST(PCI0200, VIRTIO_PCI_DEVICE_BLK,
              test_pci_remove_during_kick,
              "boot blk survives PCI hot remove of a sibling function",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
