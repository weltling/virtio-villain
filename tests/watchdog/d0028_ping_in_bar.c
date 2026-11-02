/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0028: Watchdog ping descriptor pointing at device MMIO BAR.
 *
 * The watchdog ping is a writable buffer the device
 * fills with a counter byte to acknowledge the kick. Submit a
 * ping whose writable descriptor addr points at the device's
 * own common configuration BAR rather than RAM. A device that
 * writes the ack through the generic memory API without
 * validating the target region can wedge or corrupt its own
 * registers. The device must reject or handle the non RAM
 * target cleanly.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_ping_in_bar(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, mmio_phys, 4, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0028, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_ping_in_bar,
              "Watchdog ping descriptor in device MMIO BAR",
              VIRTIO_SPEC_V1_2, "-");
