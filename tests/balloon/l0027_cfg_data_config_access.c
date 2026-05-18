/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0027: Balloon config access through PCI cfg data capability.
 *
 * Spec 4.1.4.9.2, 5.5.4: Access the balloon device config space
 * through the PCI cfg access capability at various offsets.
 * Adapted from QEMU OSS-fuzz #71649 where a specific cfg_data
 * access pattern triggered an assertion in virtio_address_space_lookup.
 * The device must handle cfg_data routed config reads at boundary
 * offsets without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <unistd.h>
#include <fcntl.h>

static test_result_t test_balloon_cfg_data_config(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;

    if (dev->pci_cfg_cap_offset == 0)
        return TEST_SKIP;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap = dev->pci_cfg_cap_offset;

    /*
     * Access device config BAR at offset 0 (num_pages field).
     * Standard 4 byte read through cfg_data.
     */
    pci_cfg_write8(fd, cap + 4, 0);  /* BAR 0 */
    pci_cfg_write32(fd, cap + 8, 0); /* offset 0 */
    pci_cfg_write32(fd, cap + 12, 4); /* length 4 */
    volatile uint32_t val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    /*
     * Read at device_cfg_length boundary. If device config is
     * smaller than offset, the device must not access past its
     * config region.
     */
    uint32_t cfg_len = dev->device_cfg_length;
    if (cfg_len > 0) {
        pci_cfg_write8(fd, cap + 4, 0);
        pci_cfg_write32(fd, cap + 8, cfg_len - 1);
        pci_cfg_write32(fd, cap + 12, 4); /* extends 3 bytes past */
        val = pci_cfg_read32(fd, cap + 16);
        (void)val;
    }

    /*
     * Access at offset past actual config length.
     * Triggers the address space lookup path with potentially
     * unmapped regions.
     */
    pci_cfg_write8(fd, cap + 4, 0);
    pci_cfg_write32(fd, cap + 8, cfg_len + 0x100);
    pci_cfg_write32(fd, cap + 12, 4);
    val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    /* Write through cfg_data past config boundary */
    pci_cfg_write8(fd, cap + 4, 0);
    pci_cfg_write32(fd, cap + 8, cfg_len + 0x100);
    pci_cfg_write32(fd, cap + 12, 4);
    pci_cfg_write32(fd, cap + 16, 0x2);

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("device_status is zero after cfg_data access");
    return TEST_PASS;
}

REGISTER_TEST(L0027, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_cfg_data_config,
              "Balloon config access via PCI cfg data capability",
              VIRTIO_SPEC_V1_2, "4.1.4.9.2");
