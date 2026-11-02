/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0104: PCI revision ID is 1 for modern virtio.
 *
 * Spec 4.1.2.1: Non-transitional virtio PCI devices have revision
 * ID 0x01 in PCI config space at offset 0x08. Verify it.
 */
#include "lib/virtio_test.h"
#include "lib/pci.h"

static test_result_t test_pci_revision(struct virtio_dev *dev,
                                       struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0) return TEST_SKIP;

    uint8_t rev = pci_cfg_read8(fd, 0x08);
    close(fd);

    if (rev != 0x01)
        TFAIL("revision ID 0x%02x, expected 0x01", rev);

    return TEST_PASS;
}

REGISTER_TEST(PCI0104, VIRTIO_PCI_DEVICE_BLK, test_pci_revision,
              "PCI revision ID is 0x01 for modern virtio",
              VIRTIO_SPEC_V1_2, "4.1.2.1");
