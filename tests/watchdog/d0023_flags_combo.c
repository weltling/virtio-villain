/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0023: Watchdog ping with both readable and writable flags.
 *
 * The watchdog ping buffer should be writable only.
 * Setting both readable and writable (i.e. no WRITE flag plus
 * chaining a writable) is an unusual combination. The device
 * must not misinterpret the flags.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_flags_combo(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xAA, 8);

    /* Descriptor 0: readable (no WRITE flag) chained to writable */
    vv_desc(vr, 0, buf, 4, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf) + 4, 4,
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0023, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_flags_combo,
              "Watchdog ping with readable then writable chain",
              VIRTIO_SPEC_V1_2, "-");
