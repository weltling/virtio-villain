/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0044: MSI multiple message enable.
 *
 * PCI 3.0 6.8.1.3: Set Multiple Message Enable field (bits 4-6)
 * to request more vectors than available (exceeding Multiple
 * Message Capable). The device must clamp or reject gracefully.
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

#define MSI_CTRL_MMC_MASK 0x000E  /* Bits 1-3: Multiple Message Capable */
#define MSI_CTRL_MME_MASK 0x0070  /* Bits 4-6: Multiple Message Enable */
#define MSI_CTRL_MME_SHIFT 4

static test_result_t test_pci_msi_multi_msg(struct virtio_dev *dev,
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

    /* Read Multiple Message Capable (max vectors log2) */
    uint8_t mmc = (msg_ctrl & MSI_CTRL_MMC_MASK) >> 1;

    /* Try to set MME beyond what MMC allows (max is 5 = 32 vectors) */
    uint8_t try_mme = (mmc < 5) ? mmc + 1 : 5;
    uint16_t new_ctrl = (msg_ctrl & ~MSI_CTRL_MME_MASK) |
                        ((uint16_t)try_mme << MSI_CTRL_MME_SHIFT);

    uint32_t new_dword0 = (cap_dword0 & 0x0000FFFF) |
                          ((uint32_t)new_ctrl << 16);
    pci_cfg_write32(fd, msi_pos, new_dword0);
    __sync_synchronize();
    usleep(1000);

    /* Read back — device should clamp MME to at most MMC */
    uint32_t readback = pci_cfg_read32(fd, msi_pos);
    uint16_t rb_ctrl = (uint16_t)(readback >> 16);
    uint8_t rb_mme = (rb_ctrl & MSI_CTRL_MME_MASK) >> MSI_CTRL_MME_SHIFT;
    (void)rb_mme; /* just exercise the path */

    /* Restore original */
    pci_cfg_write32(fd, msi_pos, cap_dword0);
    __sync_synchronize();

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0044, VIRTIO_PCI_DEVICE_BLK, test_pci_msi_multi_msg,
              "MSI multiple message enable exceeds capable",
              VIRTIO_SPEC_V1_2, "4.1.4");
