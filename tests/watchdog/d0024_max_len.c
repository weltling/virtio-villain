/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0024: Watchdog ping with maximum length descriptor.
 *
 * Submit a ping with len set to 0xFFFFFFFF. Even
 * though the watchdog only needs a tiny buffer, the device must
 * not overflow internal calculations when presented with a
 * maximum length writable descriptor.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_max_len(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 0xFFFFFFFFU,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0024, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_max_len,
              "Watchdog ping with maximum descriptor length",
              VIRTIO_SPEC_V1_2, "-");
