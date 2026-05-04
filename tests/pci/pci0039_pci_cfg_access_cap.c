/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0039: Access VIRTIO_PCI_CAP_PCI_CFG alternative config access.
 *
 * Spec 4.1.4.7.2: The PCI configuration access capability allows
 * access to BAR space via PCI config space for regions that may not
 * be mapped. Write to cap.bar, cap.offset, cap.length, then read/
 * write cap.data.
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
#define PCI_CAP_ID_VNDR   0x09

static test_result_t test_pci_cfg_access_cap(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;

    /* The pci_cfg_cap_offset was found during init */
    if (!dev->pci_cfg_cap_offset)
        return TEST_SKIP;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap_off = dev->pci_cfg_cap_offset;

    /*
     * Access device_status through the PCI_CFG capability:
     * struct virtio_pci_cap {
     *   u8 cap_vndr;    // +0
     *   u8 cap_next;    // +1
     *   u8 cap_len;     // +2
     *   u8 cfg_type;    // +3
     *   u8 bar;         // +4
     *   u8 id;          // +5
     *   u8 padding[2];  // +6
     *   u32 offset;     // +8
     *   u32 length;     // +12
     * };
     * struct virtio_pci_cfg_cap {
     *   struct virtio_pci_cap cap;
     *   u8 pci_cfg_data[4]; // +16
     * };
     */

    /* Find which BAR the common config is on by reading cap.bar from
     * the COMMON_CFG capability. We already know the device is mapped
     * but let's read device_status through PCI_CFG cap for coverage. */

    /* Walk caps to find COMMON_CFG to learn which bar it's on */
    uint8_t pos = pci_cfg_read8(fd, PCI_CAP_PTR) & 0xFC;
    uint8_t common_bar = 0;
    uint32_t common_off = 0;
    int count = 0;

    while (pos && count < 48) {
        uint8_t id = pci_cfg_read8(fd, pos + PCI_CAP_LIST_ID);
        if (id == PCI_CAP_ID_VNDR) {
            uint8_t cfg_type = pci_cfg_read8(fd, pos + 3);
            if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG) {
                common_bar = pci_cfg_read8(fd, pos + 4);
                common_off = pci_cfg_read32(fd, pos + 8);
                break;
            }
        }
        pos = pci_cfg_read8(fd, pos + PCI_CAP_LIST_NEXT) & 0xFC;
        count++;
    }

    /* Use PCI_CFG cap to read device_status (offset 20 in common cfg) */
    uint32_t status_off = common_off + 20; /* device_status offset */

    pci_cfg_write8(fd, cap_off + 4, common_bar);      /* cap.bar */
    pci_cfg_write32(fd, cap_off + 8, status_off);     /* cap.offset */
    pci_cfg_write32(fd, cap_off + 12, 1);             /* cap.length = 1 byte */
    __sync_synchronize();

    /* Read through pci_cfg_data */
    uint8_t ds = pci_cfg_read8(fd, cap_off + 16);

    close(fd);

    if (ds == 0)
        TWEDGED("ds == 0");
    if (!(ds & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(ds & VIRTIO_STATUS_DRIVER_OK)");

    return TEST_PASS;
}

REGISTER_TEST(PCI0039, VIRTIO_PCI_DEVICE_BLK, test_pci_cfg_access_cap,
              "Access config via VIRTIO_PCI_CAP_PCI_CFG",
              VIRTIO_SPEC_V1_2, "4.1.4.7.2");
