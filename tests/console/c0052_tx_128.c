/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0052: console TX 128 bytes.
 *
 * Submit 128 bytes on the TX queue. Tests moderate size transmit.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_128(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 'A', 128);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 128, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0052, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_128,
              "TX 128 bytes",
              VIRTIO_SPEC_V1_2, "5.3.6", 1);
