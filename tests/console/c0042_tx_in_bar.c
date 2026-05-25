/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0042: Console transmit readable descriptor in device MMIO BAR.
 *
 * Spec 5.3: The console transmitq carries readable buffers that
 * the device drains as terminal output. Submit a TX whose
 * readable descriptor addr points at the device's own common
 * configuration BAR rather than RAM. A device that reads the
 * payload through the generic memory API without validating
 * the source region can interpret its own register layout as
 * console bytes or wedge. The device must reject or handle the
 * non RAM source cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_console_tx_in_bar(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, mmio_phys, 64, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0042, VIRTIO_PCI_DEVICE_CONSOLE,
                test_console_tx_in_bar,
                "Console TX readable descriptor in device MMIO BAR",
                VIRTIO_SPEC_V1_2, "5.3", 1);
