/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0081: Access PCI cfg data with offset beyond BAR length.
 *
 * Spec 4.1.4.9: Set the offset field to a value that exceeds the
 * BAR region size. The device must bounds check and not access
 * memory beyond the BAR mapping.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_cfg_oob_offset(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;

    if (dev->pci_cfg_cap_offset == 0)
        return TEST_SKIP;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap = dev->pci_cfg_cap_offset;

    /* Use BAR 0 (valid) but absurd offset */
    pci_cfg_write8(fd, cap + 4, 0);
    pci_cfg_write32(fd, cap + 8, 0xFFFFF000);
    pci_cfg_write32(fd, cap + 12, 4);

    /* Read through oob offset */
    volatile uint32_t val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    /* Write through oob offset */
    pci_cfg_write32(fd, cap + 16, 0x12345678);

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(PCI0081, VIRTIO_PCI_DEVICE_BLK, test_pci_cfg_oob_offset,
              "PCI cfg data access with offset beyond BAR",
              VIRTIO_SPEC_V1_2, "4.1.4.9");
