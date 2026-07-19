/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0023: Watchdog ping with both readable and writable flags.
 *
 * Spec 5.20: The watchdog ping buffer should be writable only.
 * Setting both readable and writable (i.e. no WRITE flag plus
 * chaining a writable) is an unusual combination. The device
 * must not misinterpret the flags.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_flags_combo(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xAA, 8);

    /* Descriptor 0: readable (no WRITE flag) chained to writable */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 4,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf) + 4, 4,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0023, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_flags_combo,
              "Watchdog ping with readable then writable chain",
              VIRTIO_SPEC_V1_2, "-");
