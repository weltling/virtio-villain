/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0068: desc_addr_other_device_cfg
 *
 * Packed twin of T0125. Point a device writable data descriptor at
 * another virtio device's common configuration in MMIO, the virtio
 * serial device the runner always attaches. The blk device then DMAs
 * disk bytes into the serial device's registers. A device must not DMA
 * into any device's registers; a hang or a sanitizer fault is a guest
 * triggered host problem. Skips when no second device is found.
 */
#include "tests/packed/packed_util.h"
#include "lib/pci.h"

#define PCI_CAP_ID_VNDR        0x09
#define VIRTIO_PCI_CAP_COMMON  1

static uint64_t find_other_common_cfg(void)
{
    char slot[64];
    if (pci_find_device(0x1af4, VIRTIO_PCI_DEVICE_CONSOLE,
                        slot, sizeof(slot)) != 0)
        return 0;
    pci_enable(slot);

    int fd = pci_cfg_open(slot);
    if (fd < 0)
        return 0;

    uint8_t common_bar = 0;
    uint32_t common_off = 0;
    int found = 0;
    uint8_t cap = pci_cfg_read8(fd, 0x34);
    for (int i = 0; cap && i < 48; i++) {
        if (pci_cfg_read8(fd, cap) == PCI_CAP_ID_VNDR &&
            pci_cfg_read8(fd, cap + 3) == VIRTIO_PCI_CAP_COMMON) {
            common_bar = pci_cfg_read8(fd, cap + 4);
            common_off = pci_cfg_read32(fd, cap + 8);
            found = 1;
            break;
        }
        cap = pci_cfg_read8(fd, cap + 1);
    }
    close(fd);
    if (!found)
        return 0;

    uint64_t base = pci_bar_phys(slot, common_bar);
    if (base == 0)
        return 0;
    return base + common_off;
}

static test_result_t test_packed_desc_addr_other_device_cfg(struct virtio_dev *dev,
                                                           struct vring_packed *vr)
{
    uint64_t target = find_other_common_cfg();
    if (target == 0)
        return TEST_SKIP;

    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, target, 512,
                        VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0068, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_other_device_cfg,
                     "Packed descriptor address into another device's config",
                     VIRTIO_SPEC_V1_2, "2.8.6");
