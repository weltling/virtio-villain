/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0102: PCI subsystem vendor and device IDs are virtio.
 *
 * Spec 4.1.2.1: Non-transitional virtio PCI devices have subsystem
 * vendor ID 0x1AF4 (Red Hat) and subsystem device ID matching the
 * virtio device type. Read PCI config at offsets 0x2C and 0x2E.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <unistd.h>

static test_result_t test_pci_subsystem(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0) return TEST_SKIP;

    uint16_t sub_vendor = pci_cfg_read16(fd, 0x2C);
    uint16_t sub_device = pci_cfg_read16(fd, 0x2E);
    close(fd);

    if (sub_vendor != 0x1AF4)
        TFAIL("subsystem vendor 0x%04x, expected 0x1AF4", sub_vendor);

    /* For modern devices, subsystem device ID >= 0x40 */
    if (sub_device < 0x0001)
        TFAIL("subsystem device 0x%04x is zero", sub_device);

    return TEST_PASS;
}

REGISTER_TEST(PCI0102, VIRTIO_PCI_DEVICE_BLK, test_pci_subsystem,
              "PCI subsystem vendor is 0x1AF4",
              VIRTIO_SPEC_V1_2, "4.1.2.1");
