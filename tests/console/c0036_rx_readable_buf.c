/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0036: console_rx_readable_buffer
 *
 * Post a device readable (not writable) buffer on the RX queue.
 * Spec 5.3.6.1 says RX buffers must be device writable. A buffer
 * without VRING_DESC_F_WRITE on the RX queue is malformed and
 * must be rejected or ignored.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_rx_readable(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);

    /* Omit VRING_DESC_F_WRITE intentionally */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 256, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* RX is queue 0 for console */
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0036, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_readable,
              "RX queue buffer without writable flag",
              VIRTIO_SPEC_V1_2, "5.3.6.1");
