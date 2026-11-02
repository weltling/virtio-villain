/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0009: net_rx_readable_desc
 *
 * Place a device-readable (not WRITE) descriptor in the RX queue.
 * Spec 5.1.6.4.2: driver MUST NOT place device-readable buffers into
 * the rx queue. The device must reject.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_readable_desc(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xAA, 1526);

    /*
     * RX queue descriptor WITHOUT WRITE flag.
     * Device needs to write received data here but cannot.
     */
    vv_desc(vr, 0, buf, 1526, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0009, VIRTIO_PCI_DEVICE_NET, test_net_rx_readable_desc,
              "Device-readable descriptor in RX queue",
              VIRTIO_SPEC_V1_2, "5.1.6.4", 0);
