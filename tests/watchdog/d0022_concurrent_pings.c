/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0022: Watchdog concurrent pings.
 *
 * Submit two watchdog pings in the same avail ring
 * batch. The device must handle multiple in flight pings without
 * confusion or double completion.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_concurrent_pings(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);
    memset(buf1, 0, 1);
    memset(buf2, 0, 1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf2), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0022, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_concurrent_pings,
              "Two watchdog pings submitted in one batch",
              VIRTIO_SPEC_V1_2, "-");
