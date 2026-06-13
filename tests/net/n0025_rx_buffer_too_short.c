/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0025: net_rx_buffer_too_short
 *
 * Post an RX buffer chain whose total writable length is less than
 * the virtio_net_hdr size. The device should not write partial headers
 * or crash when there is insufficient buffer space.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_buffer_too_short(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Post a 4-byte writable buffer - smaller than virtio_net_hdr (10/12) */
    vring_raw_set_desc(vr, 0, buf_phys, 4,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0025, VIRTIO_PCI_DEVICE_NET, test_net_rx_buffer_too_short,
              "RX buffer chain shorter than virtio_net_hdr",
              VIRTIO_SPEC_V1_2, "5.1.6.4.2", 0);
