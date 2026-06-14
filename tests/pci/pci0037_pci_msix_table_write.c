/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0037: MSI-X table entry manipulation.
 *
 * Spec 4.1.4.5: If MSI-X is available, write to MSI-X table entries
 * to configure message address/data and per-vector masking.
 * Exercise the MSI-X table directly through the BAR mapping.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* MSI-X table entry layout */
static test_result_t test_pci_msix_table_write(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    /* Find MSI-X capability */
    uint16_t status = pci_cfg_read16(fd, PCI_STATUS);
    if (!(status & 0x10)) {
        close(fd);
        return TEST_SKIP;
    }

    uint8_t pos = pci_cfg_read8(fd, PCI_CAP_PTR) & 0xFC;
    uint8_t msix_pos = 0;
    int count = 0;

    while (pos && count < 48) {
        uint8_t id = pci_cfg_read8(fd, pos + PCI_CAP_LIST_ID);
        if (id == PCI_CAP_ID_MSIX) {
            msix_pos = pos;
            break;
        }
        pos = pci_cfg_read8(fd, pos + PCI_CAP_LIST_NEXT) & 0xFC;
        count++;
    }

    if (!msix_pos) {
        close(fd);
        return TEST_SKIP;
    }

    /* Read MSI-X Message Control: table size */
    uint16_t msg_ctrl = pci_cfg_read16(fd, msix_pos + 2);
    uint16_t table_size = (msg_ctrl & 0x7FF) + 1;

    /* Read Table BIR and offset */
    uint32_t table_off_bir = pci_cfg_read32(fd, msix_pos + 4);
    int table_bir = table_off_bir & 0x7;
    uint32_t table_offset = table_off_bir & ~0x7u;

    /* Map the BAR containing the MSI-X table */
    volatile void *bar = pci_map_bar(dev->slot, table_bir);
    if (!bar) {
        close(fd);
        return TEST_SKIP;
    }

    volatile struct msix_table_entry *table =
        (volatile struct msix_table_entry *)((uint8_t *)bar + table_offset);

    /* Mask vector 0 */
    uint32_t orig_ctrl = table[0].vector_ctrl;
    table[0].vector_ctrl = orig_ctrl | 1;  /* set mask bit */
    __sync_synchronize();

    /* Read back */
    uint32_t readback = table[0].vector_ctrl;
    (void)readback;

    /* Write a message address (won't fire since masked) */
    table[0].msg_addr_lo = 0xFEE00000;  /* typical APIC address */
    table[0].msg_addr_hi = 0;
    table[0].msg_data = 0x30;
    __sync_synchronize();

    /* Unmask (restore) */
    table[0].vector_ctrl = orig_ctrl;
    __sync_synchronize();

    close(fd);

    /* Verify device survived */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    (void)table_size;
    return TEST_PASS;
}

REGISTER_TEST(PCI0037, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_table_write,
              "MSI-X table entry write and per-vector mask",
              VIRTIO_SPEC_V1_2, "4.1.4.5");
