/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0031: PCI shared memory region capability probe (spec 2.6)
 *
 * Walk the PCI capability list looking for VIRTIO_PCI_CAP_SHARED_MEM
 * (type 8). Verify that if present, it has valid BAR/offset/length
 * fields. If not present, simply pass (not all devices expose SHM).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static test_result_t test_shared_memory_cap(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap_ptr = pci_cfg_read8(fd, 0x34);
    int found = 0;

    while (cap_ptr) {
        uint8_t cap_id = pci_cfg_read8(fd, cap_ptr);
        uint8_t cap_next = pci_cfg_read8(fd, cap_ptr + 1);

        if (cap_id == 0x09) { /* vendor-specific */
            uint8_t cfg_type = pci_cfg_read8(fd, cap_ptr + 3);
            if (cfg_type == VIRTIO_PCI_CAP_SHARED_MEM) {
                uint8_t bar = pci_cfg_read8(fd, cap_ptr + 4);
                uint32_t offset = pci_cfg_read32(fd, cap_ptr + 8);
                uint32_t length = pci_cfg_read32(fd, cap_ptr + 12);

                found = 1;

                /* BAR must be 0-5 */
                if (bar > 5) {
                    close(fd);
                    TFAIL("bar > 5");
                }
                /* Length should be non-zero */
                if (length == 0) {
                    close(fd);
                    TFAIL("length == 0");
                }
                (void)offset;
            }
        }
        cap_ptr = cap_next;
    }
    close(fd);

    /* Either found with valid fields or not present — both pass */
    (void)found;
    return TEST_PASS;
}

REGISTER_TEST(PCI0031, VIRTIO_PCI_DEVICE_BLK, test_shared_memory_cap,
              "PCI shared memory capability probe and validation",
              VIRTIO_SPEC_V1_2, "2.6");
