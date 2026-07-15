/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0103: PCI vendor ID is 0x1AF4.
 *
 * Spec 4.1.2: All virtio PCI devices have vendor ID 0x1AF4 (Red Hat).
 * Read vendor ID from PCI config offset 0 and verify.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <unistd.h>

static test_result_t test_pci_vendor_id(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0) return TEST_SKIP;

    uint16_t vendor = pci_cfg_read16(fd, 0);
    close(fd);

    if (vendor != 0x1AF4)
        TFAIL("vendor ID 0x%04x, expected 0x1AF4", vendor);

    return TEST_PASS;
}

REGISTER_TEST(PCI0103, VIRTIO_PCI_DEVICE_BLK, test_pci_vendor_id,
              "PCI vendor ID is 0x1AF4",
              VIRTIO_SPEC_V1_2, "4.1.2");
