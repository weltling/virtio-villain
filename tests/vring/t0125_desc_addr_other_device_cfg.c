/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0125: desc_addr_other_device_cfg
 *
 * Cross device variant of T0063. Point a blk read's data descriptor at
 * another virtio device's common configuration in MMIO, the virtio
 * serial device that the runner always attaches. The blk device then
 * DMAs disk bytes into the serial device's registers, including its
 * device_status, from inside the blk I/O completion. A device must not
 * DMA into any device's registers. The per device reentrancy guard is
 * scoped to one device, so a cross device write is not covered by it. A
 * hang or a sanitizer fault is a guest triggered host problem. Skips
 * when no second virtio device or its common config cap is found.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/pci.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

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

static test_result_t test_desc_addr_other_device_cfg(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint64_t target = find_other_common_cfg();
    if (target == 0)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* Data descriptor targets another device's common config MMIO. */
    vring_raw_set_desc(vr, 1, target, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a cross device config write");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0125, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_other_device_cfg,
              "Descriptor address into another device's common config",
              VIRTIO_SPEC_V1_2, "2.7.5");
