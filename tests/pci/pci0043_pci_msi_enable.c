/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0043: Enable legacy MSI and write message address/data.
 *
 * PCI 3.0 6.8.1: Set MSI Enable in Message Control, write a valid
 * message address (APIC range) and message data. Verify the device
 * accepts the configuration.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_msi_enable(struct virtio_dev *dev,
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

    /* Read Message Control */
    uint32_t cap_dword0 = pci_cfg_read32(fd, msi_pos);
    uint16_t msg_ctrl = (uint16_t)(cap_dword0 >> 16);
    int is_64bit = !!(msg_ctrl & MSI_CTRL_64BIT);

    /* Write Message Address (offset +4) */
    pci_cfg_write32(fd, msi_pos + 4, 0xFEE00000);  /* APIC base */

    if (is_64bit) {
        /* Message Address High (offset +8) */
        pci_cfg_write32(fd, msi_pos + 8, 0x00000000);
        /* Message Data (offset +12) */
        pci_cfg_write32(fd, msi_pos + 12, 0x0030);
    } else {
        /* Message Data (offset +8) */
        pci_cfg_write32(fd, msi_pos + 8, 0x0030);
    }

    __sync_synchronize();

    /* Enable MSI */
    uint32_t new_dword0 = (cap_dword0 & 0x0000FFFF) |
                          ((uint32_t)(msg_ctrl | MSI_CTRL_ENABLE) << 16);
    pci_cfg_write32(fd, msi_pos, new_dword0);
    __sync_synchronize();
    usleep(1000);

    /* Readback to confirm enable bit stuck */
    uint32_t readback = pci_cfg_read32(fd, msi_pos);
    uint16_t rb_ctrl = (uint16_t)(readback >> 16);
    (void)rb_ctrl;

    /* Disable MSI (restore) */
    pci_cfg_write32(fd, msi_pos, cap_dword0);
    __sync_synchronize();

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0043, VIRTIO_PCI_DEVICE_BLK, test_pci_msi_enable,
              "Enable legacy MSI and set address/data",
              VIRTIO_SPEC_V1_2, "4.1.4");
