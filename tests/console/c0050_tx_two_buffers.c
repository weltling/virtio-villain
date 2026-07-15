/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0050: console TX two buffers in one batch.
 *
 * Submit two TX buffers to the console transmit queue in one avail
 * ring update. Both must be consumed. Tests that the console device
 * handles multiple pending transmit buffers per notification.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_batch(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);

    memcpy(buf1, "hello\n", 6);
    memcpy(buf2, "world\n", 6);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf1), 6, 0, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf2), 6, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0050, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_batch,
              "TX two buffers in one batch",
              VIRTIO_SPEC_V1_2, "5.3.6", 1);
