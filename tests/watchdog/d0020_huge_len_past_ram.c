/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0020: watchdog ping with writable len past end of guest RAM.
 *
 * Same shape as RNG0004: program a writable descriptor whose base
 * lives in valid guest RAM but whose length crosses the end of all
 * System RAM. The device must not access memory outside the
 * guest's mapping or crash the VMM.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_huge_len_past_ram(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    uint64_t buf_phys;
    uint8_t *buf = vv_alloc_page_near_ram_top(ram_top, &buf_phys);
    if (!buf)
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - buf_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, buf_phys, len, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0020, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_huge_len_past_ram,
              "Watchdog ping writable len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "-");
