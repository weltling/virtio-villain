/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0009: net_rx_readable_desc
 *
 * Place a device-readable (not WRITE) descriptor in the RX queue.
 * Spec 5.1.6.4.2: driver MUST NOT place device-readable buffers into
 * the rx queue. The device must reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_readable_desc(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xAA, 1526);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /*
     * RX queue descriptor WITHOUT WRITE flag.
     * Device needs to write received data here but cannot.
     */
    vring_raw_set_desc(vr, 0, buf_phys, 1526, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0009, VIRTIO_PCI_DEVICE_NET, test_net_rx_readable_desc,
              "Device-readable descriptor in RX queue",
              VIRTIO_SPEC_V1_2, "5.1.6.4", 0);
