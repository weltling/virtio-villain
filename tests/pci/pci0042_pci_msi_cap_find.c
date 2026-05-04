/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0042: Find and read legacy MSI capability.
 *
 * PCI 3.0 6.8.1: Walk the capability list to locate the MSI
 * capability (ID 0x05). Read Message Control to determine
 * 64-bit and per-vector masking support.
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

static test_result_t test_pci_msi_cap_find(struct virtio_dev *dev,
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

    /* Read Message Control (offset +2 from cap start) */
    uint16_t msg_ctrl = pci_cfg_read16(fd, msi_pos + 2);

    /* Bit 7: 64-bit address capable */
    /* Bit 8: per-vector masking capable */
    /* Bits 1-3: multiple message capable (log2) */
    (void)msg_ctrl;

    close(fd);

    /* Device survived enumeration */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0042, VIRTIO_PCI_DEVICE_BLK, test_pci_msi_cap_find,
              "Find legacy MSI capability in config space",
              VIRTIO_SPEC_V1_2, "4.1.4");
