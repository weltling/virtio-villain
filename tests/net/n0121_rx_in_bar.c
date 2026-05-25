/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0121: Net RX writable descriptor in device MMIO BAR.
 *
 * Spec 5.1.6: The net receive queue carries writable buffers
 * the device fills with inbound packets. Submit an RX whose
 * writable descriptor addr points at the device's own common
 * configuration BAR rather than RAM. A device that writes
 * packet bytes through the generic memory API without
 * validating the target region can corrupt its own registers
 * or wedge when a frame arrives. The device must reject or
 * handle the non RAM target cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_net_rx_in_bar(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, mmio_phys, 1500, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0121, VIRTIO_PCI_DEVICE_NET, test_net_rx_in_bar,
              "Net RX writable descriptor in device MMIO BAR",
              VIRTIO_SPEC_V1_2, "5.1.6");
