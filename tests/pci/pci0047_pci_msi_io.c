/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0047: Enable MSI then perform I/O.
 *
 * PCI 3.0 6.8.1: Enable MSI, configure a valid message address/data,
 * submit a block read and verify completion. This exercises the VMM's
 * MSI interrupt delivery path end-to-end.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_msi_io(struct virtio_dev *dev,
                                     struct vring *vr)
{
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
    int is_64bit = !!(msg_ctrl & MSI_CTRL_64BIT);

    /* Configure MSI: APIC address + vector data */
    pci_cfg_write32(fd, msi_pos + 4, 0xFEE00000);
    if (is_64bit) {
        pci_cfg_write32(fd, msi_pos + 8, 0x00000000);
        pci_cfg_write32(fd, msi_pos + 12, 0x0030);
    } else {
        pci_cfg_write32(fd, msi_pos + 8, 0x0030);
    }

    /* Enable MSI */
    uint32_t new_dword0 = (cap_dword0 & 0x0000FFFF) |
                          ((uint32_t)(msg_ctrl | MSI_CTRL_ENABLE) << 16);
    pci_cfg_write32(fd, msi_pos, new_dword0);
    __sync_synchronize();

    /* Now do a block read */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *sts = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *sts = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(sts), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t result = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);

    /* Disable MSI (restore) */
    pci_cfg_write32(fd, msi_pos, cap_dword0);
    __sync_synchronize();

    close(fd);
    return result;
}

REGISTER_TEST(PCI0047, VIRTIO_PCI_DEVICE_BLK, test_pci_msi_io,
              "Enable MSI then complete block I/O",
              VIRTIO_SPEC_V1_2, "4.1.4");
