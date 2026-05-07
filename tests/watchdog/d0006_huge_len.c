/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0006: Watchdog ping with oversized writable length.
 *
 * Submit a writable ping descriptor whose length field claims 1 GiB
 * while the underlying allocation is one page. The device must clamp
 * the write to the real buffer size or refuse the descriptor; it
 * must not write past the page.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_huge_len(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 1u << 30, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0006, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_huge_len,
              "Watchdog ping with 1 GiB length",
              VIRTIO_SPEC_V1_2, "5.20");
