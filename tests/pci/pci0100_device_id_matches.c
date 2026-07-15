/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0100: PCI device ID matches virtio spec device type.
 *
 * Spec 4.1.2.1: Virtio PCI device IDs are 0x1040 + device_type_id.
 * Read the PCI device ID from config space and verify it matches
 * the expected value for a block device (0x1042).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_device_id(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint16_t device_id = pci_cfg_read16(fd, 2);  /* PCI config offset 2 */
    close(fd);

    /* For virtio 1.0+ non-transitional: device_id = 0x1040 + type */
    if (device_id < 0x1040 || device_id > 0x107F)
        TFAIL("device_id 0x%04x not in virtio range 0x1040..0x107F",
              device_id);

    /* For blk device specifically: should be 0x1042 (0x1040 + 2) */
    if (device_id != VIRTIO_PCI_DEVICE_BLK)
        TFAIL("device_id 0x%04x, expected 0x%04x for block",
              device_id, VIRTIO_PCI_DEVICE_BLK);

    return TEST_PASS;
}

REGISTER_TEST(PCI0100, VIRTIO_PCI_DEVICE_BLK, test_pci_device_id,
              "PCI device ID matches virtio block type",
              VIRTIO_SPEC_V1_2, "4.1.2.1");
