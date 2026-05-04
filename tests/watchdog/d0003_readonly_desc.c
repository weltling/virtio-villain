/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0003: Watchdog ping with read-only descriptor.
 *
 * Submit a non-writable descriptor to the watchdog queue. The device
 * expects writable descriptors for ping responses and must handle
 * this error gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_readonly(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    *buf = 0;

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Descriptor without WRITE flag — device expects writable */
    vring_raw_set_desc(vr, 0, buf_phys, 1, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0003, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_readonly,
              "Watchdog ping with non-writable descriptor",
              VIRTIO_SPEC_V1_2, "5.16");
