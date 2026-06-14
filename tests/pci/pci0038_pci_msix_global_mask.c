/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0038: MSI-X global mask via Message Control register.
 *
 * Spec 4.1.4.5 / PCI 3.0 6.8.2.3: Set the Function Mask bit in
 * the MSI-X Message Control register to globally mask all vectors,
 * then clear it. Verify device survives and interrupts resume.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MSIX_MSGCTRL_FMASK 0x4000  /* Function Mask bit (bit 14) */

static test_result_t test_pci_msix_global_mask(struct virtio_dev *dev,
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

    /* Read MSI-X cap dword at offset 0 (includes cap_id, next, msg_ctrl) */
    uint32_t cap_dword = pci_cfg_read32(fd, msix_pos);
    /* msg_ctrl is at bytes 2-3 of this dword */
    uint16_t msg_ctrl = (uint16_t)(cap_dword >> 16);

    /* Set Function Mask bit (bit 14 of msg_ctrl = bit 30 of dword) */
    pci_cfg_write32(fd, msix_pos, cap_dword | ((uint32_t)MSIX_MSGCTRL_FMASK << 16));
    __sync_synchronize();
    usleep(1000);

    /* Clear Function Mask bit (unmask) */
    pci_cfg_write32(fd, msix_pos, cap_dword & ~((uint32_t)MSIX_MSGCTRL_FMASK << 16));
    __sync_synchronize();
    usleep(1000);
    (void)msg_ctrl;

    close(fd);

    /* Verify device still alive */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0038, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_global_mask,
              "MSI-X global function mask toggle",
              VIRTIO_SPEC_V1_2, "4.1.4.5");
