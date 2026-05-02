/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0010: pci_cfg_data_no_cap
 *
 * Access the pci_cfg_data field of the VIRTIO_PCI_CAP_PCI_CFG capability
 * without first setting valid BAR and offset values. The spec says the
 * driver MUST NOT read or write pci_cfg_data unless valid cap parameters
 * are configured (4.1.4.9.2).
 *
 * This exercises the VMM's PCI config space access capability handling.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <unistd.h>

#define VIRTIO_PCI_CAP_PCI_CFG 5

static test_result_t test_pci_cfg_data_no_cap(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    /* Find VIRTIO_PCI_CAP_PCI_CFG capability */
    uint8_t cap_ptr = pci_cfg_read8(fd, 0x34);
    uint8_t cfg_cap_offset = 0;

    while (cap_ptr) {
        uint8_t cap_id = pci_cfg_read8(fd, cap_ptr);
        uint8_t cap_next = pci_cfg_read8(fd, cap_ptr + 1);
        if (cap_id == 0x09) { /* Vendor-specific */
            uint8_t cfg_type = pci_cfg_read8(fd, cap_ptr + 3);
            if (cfg_type == VIRTIO_PCI_CAP_PCI_CFG) {
                cfg_cap_offset = cap_ptr;
                break;
            }
        }
        cap_ptr = cap_next;
    }

    if (!cfg_cap_offset) {
        close(fd);
        return TEST_SKIP; /* No PCI_CFG cap found */
    }

    /*
     * Write invalid BAR number (0xFF) and offset (0xFFFFFFFF) to the cap,
     * then try to read/write pci_cfg_data.
     */
    /* cap+4 = bar, cap+8 = offset, cap+12 = length, cap+16 = data */
    pci_cfg_write8(fd, cfg_cap_offset + 4, 0xFF); /* invalid BAR */
    pci_cfg_write32(fd, cfg_cap_offset + 8, 0xFFFFFFFF); /* invalid offset */
    pci_cfg_write32(fd, cfg_cap_offset + 12, 4); /* length */

    /* Read pci_cfg_data with invalid BAR/offset */
    uint32_t val = pci_cfg_read32(fd, cfg_cap_offset + 16);
    (void)val;

    /* Write to pci_cfg_data with invalid BAR/offset */
    pci_cfg_write32(fd, cfg_cap_offset + 16, 0xDEADBEEF);

    usleep(50000);

    /* Try with BAR=0 but absurdly large offset */
    pci_cfg_write8(fd, cfg_cap_offset + 4, 0);
    pci_cfg_write32(fd, cfg_cap_offset + 8, 0x7FFFFFFF);
    pci_cfg_write32(fd, cfg_cap_offset + 12, 4);
    val = pci_cfg_read32(fd, cfg_cap_offset + 16);
    (void)val;

    usleep(50000);
    close(fd);

    /* Verify device is still alive */
    dev->common->queue_select = 0;
    __sync_synchronize();
    if (dev->common->queue_size == 0)
        TFAIL("dev->common->queue_size == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0010, VIRTIO_PCI_DEVICE_BLK, test_pci_cfg_data_no_cap,
              "Read/write pci_cfg_data with invalid BAR/offset",
              VIRTIO_SPEC_V1_2, "4.1.4.9.2");
