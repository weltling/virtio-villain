/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0051: console TX single byte.
 *
 * Submit one byte on the TX queue. Tests the minimum valid
 * transmit buffer size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_console_tx_1byte(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    buf[0] = 'X';

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 1, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0051, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_1byte,
              "TX single byte",
              VIRTIO_SPEC_V1_2, "5.3.6", 1);
