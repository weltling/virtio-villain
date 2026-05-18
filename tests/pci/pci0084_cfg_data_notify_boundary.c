/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0084: PCI cfg data access spanning notify BAR boundary.
 *
 * Spec 4.1.4.9: The PCI cfg access capability allows the driver
 * to read/write device regions through config space. Program an
 * offset within the notify BAR such that offset + length exceeds
 * the notify region size. This exercises the MemoryRegionCache
 * boundary path where a partially valid offset with a length that
 * extends past the BAR triggers an OOB in VMMs that cache the
 * BAR mapping without bounds checking the combined range.
 * Adapted from QEMU fuzz-virtio-scsi-test MemoryRegionCache OOB.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_cfg_data_notify_boundary(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    (void)vr;

    if (dev->pci_cfg_cap_offset == 0)
        return TEST_SKIP;

    if (dev->notify_length == 0)
        return TEST_SKIP;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap = dev->pci_cfg_cap_offset;
    uint32_t nlen = dev->notify_length;

    /*
     * Access 1: Read at offset = notify_length - 2 with length = 4.
     * The first 2 bytes are within the BAR, the last 2 extend past it.
     * A VMM that blindly caches the region at this size will read OOB.
     */
    pci_cfg_write8(fd, cap + 4, 0);           /* BAR 0 */
    pci_cfg_write32(fd, cap + 8, nlen - 2);   /* offset near end */
    pci_cfg_write32(fd, cap + 12, 4);         /* length spans boundary */
    volatile uint32_t val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    /*
     * Access 2: Write at offset = notify_length - 1 with length = 4.
     * Only 1 byte is inside the BAR.
     */
    pci_cfg_write8(fd, cap + 4, 0);
    pci_cfg_write32(fd, cap + 8, nlen - 1);
    pci_cfg_write32(fd, cap + 12, 4);
    pci_cfg_write32(fd, cap + 16, 0x00010000);

    /*
     * Access 3: Exactly at notify_length (fully OOB but offset alone
     * might pass a size < check if size is not subtracted from offset).
     */
    pci_cfg_write8(fd, cap + 4, 0);
    pci_cfg_write32(fd, cap + 8, nlen);
    pci_cfg_write32(fd, cap + 12, 4);
    val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("device_status is zero after boundary cfg_data access");
    return TEST_PASS;
}

REGISTER_TEST(PCI0084, VIRTIO_PCI_DEVICE_BLK,
              test_pci_cfg_data_notify_boundary,
              "PCI cfg data access spanning notify BAR boundary",
              VIRTIO_SPEC_V1_2, "4.1.4.9");
