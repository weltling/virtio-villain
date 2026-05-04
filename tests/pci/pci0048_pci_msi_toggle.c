/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0048: Toggle MSI enable rapidly.
 *
 * PCI 3.0 6.8.1: Rapidly enable and disable MSI to stress the
 * VMM's MSI state machine transitions. The device must not wedge
 * or corrupt state under rapid toggling.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define PCI_CAP_LIST_ID   0
#define PCI_CAP_LIST_NEXT 1
#define PCI_CAP_PTR       0x34
#define PCI_STATUS        0x06
#define PCI_CAP_ID_MSI    0x05

#define MSI_CTRL_ENABLE   0x0001

static test_result_t test_pci_msi_toggle(struct virtio_dev *dev,
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

    uint32_t cap_dword0 = pci_cfg_read32(fd, msi_pos);
    uint16_t msg_ctrl = (uint16_t)(cap_dword0 >> 16);

    uint32_t enabled = (cap_dword0 & 0x0000FFFF) |
                       ((uint32_t)(msg_ctrl | MSI_CTRL_ENABLE) << 16);
    uint32_t disabled = cap_dword0;  /* original = disabled */

    /* Rapid toggle 32 times */
    for (int i = 0; i < 32; i++) {
        pci_cfg_write32(fd, msi_pos, enabled);
        __sync_synchronize();
        pci_cfg_write32(fd, msi_pos, disabled);
        __sync_synchronize();
    }

    /* Final state: disabled */
    pci_cfg_write32(fd, msi_pos, disabled);
    __sync_synchronize();
    usleep(1000);

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0048, VIRTIO_PCI_DEVICE_BLK, test_pci_msi_toggle,
              "Rapid MSI enable/disable toggling",
              VIRTIO_SPEC_V1_2, "4.1.4");
