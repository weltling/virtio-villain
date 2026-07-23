/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0118: sriov_cap_present
 *
 * When VIRTIO_F_SR_IOV (bit 37) is offered, the device MUST present a
 * PCI SR-IOV capability structure. Spec 6 Reserved Feature Bits: "A
 * device SHOULD offer VIRTIO_F_SR_IOV if it is a PCI device and
 * presents a PCI SR-IOV capability structure, otherwise it MUST NOT
 * offer VIRTIO_F_SR_IOV". Walk the PCIe extended capability list and
 * confirm the SR-IOV capability (extended ID 0x0010) is present.
 * Skips when the device does not offer the feature.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <unistd.h>

#define PCIE_EXT_CAP_START     0x100
#define PCIE_EXT_CAP_ID_SRIOV  0x0010

static test_result_t test_pci_sriov_cap_present(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_F_SR_IOV))
        return TEST_SKIP;

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    /* Walk the PCIe extended capability list starting at 0x100. Each
     * header packs the cap ID in bits 0..15 and the next offset in
     * bits 20..31. */
    uint32_t pos = PCIE_EXT_CAP_START;
    int found = 0;
    int steps = 0;
    while (pos >= PCIE_EXT_CAP_START && steps < 256) {
        uint32_t hdr = pci_cfg_read32(fd, pos);
        if (hdr == 0 || hdr == 0xffffffffu)
            break;
        if ((hdr & 0xffff) == PCIE_EXT_CAP_ID_SRIOV) {
            found = 1;
            break;
        }
        uint32_t next = (hdr >> 20) & 0xffc;
        if (next < PCIE_EXT_CAP_START)
            break;
        pos = next;
        steps++;
    }

    close(fd);

    if (!found)
        TFAIL("SR_IOV offered but no SR-IOV PCIe capability present");

    return TEST_PASS;
}

REGISTER_TEST(PCI0118, VIRTIO_PCI_DEVICE_BLK, test_pci_sriov_cap_present,
              "SR_IOV offered implies an SR-IOV PCIe capability",
              VIRTIO_SPEC_V1_2, "6");
