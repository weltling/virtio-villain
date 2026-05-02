/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0023: net_rx_zero_length_buffer
 *
 * Place a zero-length device-writable buffer in the receive queue.
 * The device needs space to write received packets; a zero-length
 * buffer may cause an underflow in size calculations or an attempt
 * to write to zero bytes of memory.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_zero_length(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Place a zero-length writable descriptor in the queue */
    vring_raw_set_desc(vr, 0, buf_phys, 0, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0023, VIRTIO_PCI_DEVICE_NET, test_net_rx_zero_length,
              "Zero-length device-writable buffer in receiveq",
              VIRTIO_SPEC_V1_2, "5.1.6.4.2", 0);
