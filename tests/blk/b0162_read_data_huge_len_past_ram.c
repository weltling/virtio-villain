/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0162: blk read with writable data len past end of guest RAM.
 *
 * Same shape as RNG0004: submit a READ at sector 0 whose data
 * descriptor base lives in valid guest RAM but whose length
 * crosses the end of all System RAM. Device must not access
 * memory outside the guest's mapping or crash the VMM.
 *
 * Spec 5.2.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static test_result_t test_blk_read_huge_len_past_ram(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);
    if (data_phys >= ram_top)
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - data_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, len,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0162, VIRTIO_PCI_DEVICE_BLK, test_blk_read_huge_len_past_ram,
              "Blk read writable data len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "5.2.6");
