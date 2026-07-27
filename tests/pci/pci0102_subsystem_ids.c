/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0102: PCI subsystem IDs are informational for modern devices.
 *
 * Spec 4.1.2.1 places no MUST-level requirement on the PCI Subsystem
 * Vendor ID or Subsystem Device ID of a non-transitional device. The
 * subsystem fields MAY reflect the vendor and device ID of the
 * environment, and 4.1.2.2 states that drivers MAY match any subsystem
 * vendor or device ID. So a modern device may use a vendor-specific
 * subsystem vendor ID to identify its implementation; only the PCI
 * Vendor ID 0x1AF4 and the PCI Device ID are constrained at MUST level.
 *
 * The only subsystem guidance for a non-transitional device is the
 * SHOULD in 4.1.2.1 that the Subsystem Device ID be 0x40 or higher, to
 * reduce the chance of a legacy driver attaching. This test reads the
 * subsystem fields at offsets 0x2C and 0x2E and reports them, treating
 * the SHOULD as a non-fatal note. It does not fail on the subsystem
 * vendor ID.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <unistd.h>

static test_result_t test_pci_subsystem(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0) return TEST_SKIP;

    uint16_t sub_vendor = pci_cfg_read16(fd, 0x2C);
    uint16_t sub_device = pci_cfg_read16(fd, 0x2E);
    close(fd);

    /*
     * Subsystem device ID 0x40 or higher is a SHOULD, not a MUST, so a
     * lower value is reported but does not fail the test.
     */
    if (sub_device < 0x40)
        printf("vv-note %s:%d: subsystem device 0x%04x below 0x40 "
               "(SHOULD per 4.1.2.1)\n", __FILE__, __LINE__, sub_device);

    printf("vv-note %s:%d: subsystem vendor 0x%04x device 0x%04x\n",
           __FILE__, __LINE__, sub_vendor, sub_device);
    fflush(stdout);

    return TEST_PASS;
}

REGISTER_TEST(PCI0102, VIRTIO_PCI_DEVICE_BLK, test_pci_subsystem,
              "PCI subsystem IDs are informational for modern devices",
              VIRTIO_SPEC_V1_2, "4.1.2.1");
