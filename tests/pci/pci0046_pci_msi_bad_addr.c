/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0046: MSI address write with invalid (non-APIC) address.
 *
 * PCI 3.0 6.8.1: Write a non-standard message address to the MSI
 * Message Address register. The device/VMM must handle this without
 * crashing (may silently drop or reject).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_msi_bad_addr(struct virtio_dev *dev,
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
    uint8_t msi_pos = 0;
    int count = 0;

    while (pos && count < 48) {
        uint8_t id = pci_cfg_read8(fd, pos + PCI_CAP_LIST_ID);
        if (id == PCI_CAP_ID_MSI) {
            msi_pos = pos;
            break;
        }
        pos = pci_cfg_read8(fd, pos + PCI_CAP_LIST_NEXT) & 0xFC;
        count++;
    }

    if (!msi_pos) {
        close(fd);
        return TEST_SKIP;
    }

    /* Save original Message Address */
    uint32_t orig_addr = pci_cfg_read32(fd, msi_pos + 4);

    /* Write various invalid addresses */
    pci_cfg_write32(fd, msi_pos + 4, 0x00000000);  /* null */
    __sync_synchronize();
    usleep(500);

    pci_cfg_write32(fd, msi_pos + 4, 0xDEADBEEF);  /* random non-APIC */
    __sync_synchronize();
    usleep(500);

    pci_cfg_write32(fd, msi_pos + 4, 0xFFFFFFFF);  /* all ones */
    __sync_synchronize();
    usleep(500);

    /* Restore */
    pci_cfg_write32(fd, msi_pos + 4, orig_addr);
    __sync_synchronize();

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0046, VIRTIO_PCI_DEVICE_BLK, test_pci_msi_bad_addr,
              "MSI message address write with invalid values",
              VIRTIO_SPEC_V1_2, "4.1.4");
