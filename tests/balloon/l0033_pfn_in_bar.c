/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0033: Balloon inflate with PFN list pointing at the device MMIO BAR.
 *
 * Spec 5.5.6.1: The driver supplies a readable buffer holding an
 * array of 4 byte PFN values. Submit an inflate whose readable
 * descriptor points at the device's own common configuration
 * BAR rather than RAM. A device that reads the PFN list through
 * the generic memory API without validating the source region
 * can interpret its own register layout as PFNs and attempt to
 * reclaim those host pages. The device must reject or handle
 * the non RAM source cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_balloon_pfn_in_bar(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, mmio_phys, 16, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0033, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_pfn_in_bar,
              "Inflate PFN list points at device MMIO BAR",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
