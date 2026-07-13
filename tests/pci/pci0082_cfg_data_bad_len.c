/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0082: PCI cfg data with mismatched cap_len values.
 *
 * Spec 4.1.4.9: The length field specifies 1, 2, or 4 byte
 * accesses. Set length to 3 (invalid) and attempt a read/write.
 * The device must reject the malformed access.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <unistd.h>
#include <fcntl.h>

static test_result_t test_pci_cfg_bad_len(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;

    if (dev->pci_cfg_cap_offset == 0)
        return TEST_SKIP;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap = dev->pci_cfg_cap_offset;

    /* BAR 0, offset 0, but length 3 (invalid) */
    pci_cfg_write8(fd, cap + 4, 0);
    pci_cfg_write32(fd, cap + 8, 0);
    pci_cfg_write32(fd, cap + 12, 3);

    volatile uint32_t val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    /* Try length 0 */
    pci_cfg_write32(fd, cap + 12, 0);
    val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    /* Try length 8 */
    pci_cfg_write32(fd, cap + 12, 8);
    val = pci_cfg_read32(fd, cap + 16);
    (void)val;

    close(fd);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_FLAGS(PCI0082, VIRTIO_PCI_DEVICE_BLK, test_pci_cfg_bad_len,
              "PCI cfg data with invalid length field",
              VIRTIO_SPEC_V1_2, "4.1.4.9",
              TEST_FLAG_NEEDS_CFG);
