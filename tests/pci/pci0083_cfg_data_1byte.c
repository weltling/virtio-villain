/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0083: PCI cfg data read and write with valid 1 byte access.
 *
 * Spec 4.1.4.9: Set length=1, BAR=0, offset=0 and perform a
 * single byte read and write through the cfg data register.
 * Verifies the basic path works without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_cfg_1byte(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;

    if (dev->pci_cfg_cap_offset == 0)
        return TEST_SKIP;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap = dev->pci_cfg_cap_offset;

    /* 1 byte access at BAR 0, offset 0 */
    pci_cfg_write8(fd, cap + 4, 0);
    pci_cfg_write32(fd, cap + 8, 0);
    pci_cfg_write32(fd, cap + 12, 1);

    uint8_t val = pci_cfg_read8(fd, cap + 16);
    (void)val;

    /* Write a byte */
    pci_cfg_write8(fd, cap + 16, 0xAA);

    /* Read back */
    val = pci_cfg_read8(fd, cap + 16);
    (void)val;

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_FLAGS(PCI0083, VIRTIO_PCI_DEVICE_BLK, test_pci_cfg_1byte,
              "PCI cfg data 1 byte read and write",
              VIRTIO_SPEC_V1_2, "4.1.4.9",
              TEST_FLAG_NEEDS_CFG);
