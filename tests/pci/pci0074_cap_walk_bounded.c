/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0074: pci_cap_walk_bounded
 *
 * Walk the PCI capability list with a hard upper bound and
 * confirm the walk halts at next equal to zero rather than
 * spinning. Spec 4.1.4 makes the list singly linked and
 * terminated by a zero pointer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <fcntl.h>
#include <unistd.h>

static test_result_t test_pci_cap_walk_bounded(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint16_t status = pci_cfg_read16(fd, PCI_STATUS);
    if (!(status & 0x10)) {
        close(fd);
        return TEST_SKIP;
    }

    uint8_t pos = pci_cfg_read8(fd, PCI_CAP_PTR) & 0xFC;
    int steps = 0;
    while (pos && steps < 256) {
        uint8_t next = pci_cfg_read8(fd, pos + PCI_CAP_LIST_NEXT);
        pos = next & 0xFC;
        steps++;
    }

    close(fd);

    if (steps >= 256)
        TFAIL("steps >= 256");

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0074, VIRTIO_PCI_DEVICE_BLK, test_pci_cap_walk_bounded,
              "Capability list walk halts at terminator",
              VIRTIO_SPEC_V1_2, "4.1.4");
