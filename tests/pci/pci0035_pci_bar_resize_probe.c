/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0035: Write all 1s to BAR register to probe size, then restore.
 *
 * Spec 4.1.4: Standard PCI BAR sizing mechanism. Write 0xFFFFFFFF
 * to a BAR, read back to determine size, then restore the original
 * value. The device must survive this probe sequence.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_bar_resize_probe(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    /* PCI BAR0 is at config offset 0x10 */
    uint32_t bar0_offset = 0x10;

    /* Save original BAR value */
    uint32_t orig_bar = pci_cfg_read32(fd, bar0_offset);

    /* Write all 1s to probe size */
    pci_cfg_write32(fd, bar0_offset, 0xFFFFFFFF);
    __sync_synchronize();

    /* Read back to get size mask */
    uint32_t size_mask = pci_cfg_read32(fd, bar0_offset);
    (void)size_mask; /* we just care the device survives */

    /* Restore original BAR value */
    pci_cfg_write32(fd, bar0_offset, orig_bar);
    __sync_synchronize();
    usleep(10000);

    /* Verify device is still alive by reading device_status */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    uint8_t status = cfg->device_status;

    if (status == 0)
        TWEDGED("status == 0");
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(status & VIRTIO_STATUS_DRIVER_OK)");

    close(fd);
    return TEST_PASS;
}

REGISTER_TEST(PCI0035, VIRTIO_PCI_DEVICE_BLK, test_pci_bar_resize_probe,
              "PCI BAR size probe (write all 1s then restore)",
              VIRTIO_SPEC_V1_2, "4.1.4");
