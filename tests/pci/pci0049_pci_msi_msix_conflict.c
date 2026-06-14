/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0049: MSI and MSI-X mutual exclusion.
 *
 * PCI 3.0 6.8: Only one of MSI or MSI-X should be enabled at a time.
 * Enable MSI-X first (via common_cfg msix_config), then attempt to
 * enable legacy MSI. The VMM must handle this conflict gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MSIX_CTRL_ENABLE  0x8000  /* Bit 15 of MSI-X Message Control */

static test_result_t test_pci_msi_msix_conflict(struct virtio_dev *dev,
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
    uint8_t msix_pos = 0;
    int count = 0;

    while (pos && count < 48) {
        uint8_t id = pci_cfg_read8(fd, pos + PCI_CAP_LIST_ID);
        if (id == PCI_CAP_ID_MSI)
            msi_pos = pos;
        else if (id == PCI_CAP_ID_MSIX)
            msix_pos = pos;
        pos = pci_cfg_read8(fd, pos + PCI_CAP_LIST_NEXT) & 0xFC;
        count++;
    }

    if (!msi_pos || !msix_pos) {
        close(fd);
        return TEST_SKIP;
    }

    /* Enable MSI-X first */
    uint32_t msix_dword0 = pci_cfg_read32(fd, msix_pos);
    uint32_t msix_enabled = msix_dword0 | ((uint32_t)MSIX_CTRL_ENABLE << 16);
    pci_cfg_write32(fd, msix_pos, msix_enabled);
    __sync_synchronize();
    usleep(1000);

    /* Now try to enable MSI while MSI-X is active */
    uint32_t msi_dword0 = pci_cfg_read32(fd, msi_pos);
    uint16_t msi_ctrl = (uint16_t)(msi_dword0 >> 16);
    uint32_t msi_enabled = (msi_dword0 & 0x0000FFFF) |
                           ((uint32_t)(msi_ctrl | MSI_CTRL_ENABLE) << 16);
    pci_cfg_write32(fd, msi_pos, msi_enabled);
    __sync_synchronize();
    usleep(1000);

    /* Read back both — device may refuse one */
    uint32_t rb_msi = pci_cfg_read32(fd, msi_pos);
    uint32_t rb_msix = pci_cfg_read32(fd, msix_pos);
    (void)rb_msi; (void)rb_msix;

    /* Restore: disable both */
    pci_cfg_write32(fd, msi_pos, msi_dword0);
    pci_cfg_write32(fd, msix_pos, msix_dword0);
    __sync_synchronize();
    usleep(1000);

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0049, VIRTIO_PCI_DEVICE_BLK, test_pci_msi_msix_conflict,
              "MSI and MSI-X simultaneous enable conflict",
              VIRTIO_SPEC_V1_2, "4.1.4");
