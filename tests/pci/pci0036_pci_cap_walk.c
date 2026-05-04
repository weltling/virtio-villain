/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0036: Walk all PCI capabilities in config space.
 *
 * Spec 4.1.4: Virtio structures are found via PCI capabilities.
 * Walk the capability list reading type, bar, offset, length for
 * each. The device must survive full enumeration.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* PCI capability header offsets */
#define PCI_CAP_LIST_ID   0
#define PCI_CAP_LIST_NEXT 1
#define PCI_STATUS        0x06
#define PCI_CAP_PTR       0x34
#define PCI_CAP_ID_VNDR   0x09

static test_result_t test_pci_cap_walk(struct virtio_dev *dev,
                                       struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    /* Check if capabilities are supported */
    uint16_t status = pci_cfg_read16(fd, PCI_STATUS);
    if (!(status & 0x10)) {
        close(fd);
        return TEST_SKIP; /* no capability list */
    }

    uint8_t pos = pci_cfg_read8(fd, PCI_CAP_PTR) & 0xFC;
    int cap_count = 0;
    int vendor_caps = 0;

    while (pos && cap_count < 48) {
        uint8_t id = pci_cfg_read8(fd, pos + PCI_CAP_LIST_ID);
        uint8_t next = pci_cfg_read8(fd, pos + PCI_CAP_LIST_NEXT);

        if (id == PCI_CAP_ID_VNDR) {
            /* Virtio vendor-specific: read cfg_type, bar, offset, length */
            uint8_t cfg_type = pci_cfg_read8(fd, pos + 3);
            uint8_t bar = pci_cfg_read8(fd, pos + 4);
            uint32_t offset = pci_cfg_read32(fd, pos + 8);
            uint32_t length = pci_cfg_read32(fd, pos + 12);
            (void)cfg_type; (void)bar; (void)offset; (void)length;
            vendor_caps++;
        }

        pos = next & 0xFC;
        cap_count++;
    }

    close(fd);

    /* A virtio device must have at least the common cfg capability */
    if (vendor_caps == 0)
        TFAIL("vendor_caps == 0");

    /* Verify device still healthy */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0036, VIRTIO_PCI_DEVICE_BLK, test_pci_cap_walk,
              "Walk all PCI capabilities in config space",
              VIRTIO_SPEC_V1_2, "4.1.4");
