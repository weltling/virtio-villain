/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0112: net rx writable len past end of guest RAM.
 *
 * Same shape as RNG0004: program a writable receive buffer whose
 * base lives in valid guest RAM but whose length crosses the end
 * of all System RAM. Device must not access memory outside the
 * guest's mapping or crash the VMM.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_huge_len_past_ram(struct virtio_dev *dev,
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
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0112, VIRTIO_PCI_DEVICE_NET, test_net_rx_huge_len_past_ram,
              "Net rx writable len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "5.1.6");
