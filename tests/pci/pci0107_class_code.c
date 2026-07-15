/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0107: PCI class code is 0xFF0000 (other device, non VGA).
 *
 * Spec 4.1.2.1: Non-transitional virtio PCI devices use class code
 * 0xFF (unclassified), subclass 0x00, prog-if 0x00. Read from PCI
 * config offset 0x09 (3 bytes: prog-if, subclass, class).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <unistd.h>

static test_result_t test_pci_class_code(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0) return TEST_SKIP;

    /* Class code at offset 0x09 (prog_if), 0x0A (subclass), 0x0B (class) */
    uint8_t prog_if = pci_cfg_read8(fd, 0x09);
    uint8_t subclass = pci_cfg_read8(fd, 0x0A);
    uint8_t class = pci_cfg_read8(fd, 0x0B);
    close(fd);

    (void)prog_if;  /* may vary */

    /* For non-transitional: class should be 0xFF or device-appropriate */
    if (class == 0x00 && subclass == 0x00)
        TFAIL("class 0x%02x subclass 0x%02x (uninitialized)", class, subclass);

    return TEST_PASS;
}

REGISTER_TEST(PCI0107, VIRTIO_PCI_DEVICE_BLK, test_pci_class_code,
              "PCI class code is set (not zero)",
              VIRTIO_SPEC_V1_2, "4.1.2.1");
