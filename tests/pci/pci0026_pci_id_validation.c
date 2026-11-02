/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0026: PCI vendor/device ID validation (spec 4.1.2)
 *
 * Verify the PCI device IDs match expected virtio modern conventions:
 * vendor 0x1AF4, device in the modern range 0x1040-0x107F. The
 * subsystem fields carry no MUST-level requirement per 4.1.2.1 and are
 * not checked here.
 */
#include "lib/virtio_test.h"

#include <errno.h>

static test_result_t test_pci_id_validation(struct virtio_dev *dev)
{
    /* Read PCI config via sysfs */
    char path[512];
    snprintf(path, sizeof(path), "%s/vendor", dev->slot);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        TFAIL("open %s: %s", path, strerror(errno));

    char buf[32] = {0};
    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (r <= 0)
        TFAIL("read %s returned %zd", path, r);

    unsigned long vendor = strtoul(buf, NULL, 16);
    if (vendor != 0x1AF4)
        TFAIL("vendor != 0x1AF4");

    snprintf(path, sizeof(path), "%s/device", dev->slot);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        TFAIL("open %s: %s", path, strerror(errno));
    memset(buf, 0, sizeof(buf));
    r = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (r <= 0)
        TFAIL("read %s returned %zd", path, r);

    unsigned long device = strtoul(buf, NULL, 16);
    /* Modern virtio: 0x1040 + device_type */
    if (device < 0x1040 || device > 0x107F)
        TFAIL("device < 0x1040 || device > 0x107F");

    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0026, VIRTIO_PCI_DEVICE_BLK, test_pci_id_validation,
                  "PCI vendor=0x1AF4 and modern device ID range check",
                  VIRTIO_SPEC_V1_2, "4.1.2");
