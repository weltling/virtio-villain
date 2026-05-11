/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0080: Access PCI cfg data with wrong BAR number.
 *
 * Spec 4.1.4.9: VIRTIO_PCI_CAP_PCI_CFG allows access to device
 * regions through config writes. Set the BAR field to an invalid
 * BAR index (6+). The device must ignore or reject the access
 * without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_cfg_bad_bar(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;

    if (dev->pci_cfg_cap_offset == 0)
        return TEST_SKIP;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap = dev->pci_cfg_cap_offset;

    /* Standard layout: cap+4=bar, cap+8=offset, cap+12=length, cap+16=data */
    /* Write invalid BAR index 7 */
    pci_cfg_write8(fd, cap + 4, 7);
    pci_cfg_write32(fd, cap + 8, 0);
    pci_cfg_write32(fd, cap + 12, 4);

    /* Try to read data; result is undefined but must not crash */
    volatile uint32_t val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    /* Write data through the bad BAR */
    pci_cfg_write32(fd, cap + 16, 0xDEADBEEF);

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(PCI0080, VIRTIO_PCI_DEVICE_BLK, test_pci_cfg_bad_bar,
              "PCI cfg data access with invalid BAR index",
              VIRTIO_SPEC_V1_2, "4.1.4.9");
