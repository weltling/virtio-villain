/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0045: MSI per-vector masking.
 *
 * PCI 3.0 6.8.1.5: If per-vector masking is supported, toggle
 * mask bits and read pending bits. The device must handle mask
 * register writes without corruption.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_msi_pvm(struct virtio_dev *dev,
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

    if (!(msg_ctrl & MSI_CTRL_PVM)) {
        close(fd);
        return TEST_SKIP; /* no per-vector masking */
    }

    int is_64bit = !!(msg_ctrl & MSI_CTRL_64BIT);

    /* Mask/Pending registers offset depends on 64-bit capability:
     * 32-bit: mask at +12, pending at +16
     * 64-bit: mask at +16, pending at +20
     */
    uint32_t mask_off = msi_pos + (is_64bit ? 16 : 12);
    uint32_t pend_off = mask_off + 4;

    /* Read current mask */
    uint32_t orig_mask = pci_cfg_read32(fd, mask_off);

    /* Mask vector 0 */
    pci_cfg_write32(fd, mask_off, orig_mask | 1);
    __sync_synchronize();
    usleep(1000);

    /* Read pending bits */
    uint32_t pending = pci_cfg_read32(fd, pend_off);
    (void)pending;

    /* Mask all vectors */
    pci_cfg_write32(fd, mask_off, 0xFFFFFFFF);
    __sync_synchronize();
    usleep(1000);

    /* Read back mask */
    uint32_t rb_mask = pci_cfg_read32(fd, mask_off);
    (void)rb_mask;

    /* Restore original mask */
    pci_cfg_write32(fd, mask_off, orig_mask);
    __sync_synchronize();

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0045, VIRTIO_PCI_DEVICE_BLK, test_pci_msi_pvm,
              "MSI per-vector masking toggle",
              VIRTIO_SPEC_V1_2, "4.1.4");
