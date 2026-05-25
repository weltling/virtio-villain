/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0022: RNG writable descriptor pointing at the device MMIO BAR.
 *
 * Spec 5.4.6: The driver supplies a writable buffer; the device
 * fills it with random bytes. Submit an entropy request whose
 * writable descriptor points at the device's own common
 * configuration BAR rather than RAM. A device that writes the
 * entropy bytes through the generic memory API without
 * validating the target region can corrupt its own registers
 * or wedge. The device must reject or handle the non RAM target
 * cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_rng_buf_in_bar(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, mmio_phys, 32, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0022, VIRTIO_PCI_DEVICE_RNG, test_rng_buf_in_bar,
              "RNG writable descriptor points at device MMIO BAR",
              VIRTIO_SPEC_V1_2, "5.4.6");
