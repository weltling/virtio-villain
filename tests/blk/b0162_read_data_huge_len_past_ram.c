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
#include "lib/virtio_test.h"

static test_result_t test_blk_read_huge_len_past_ram(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t status_phys = vv_virt_to_phys(status);

    uint64_t data_phys;
    if (!vv_alloc_page_near_ram_top(ram_top, &data_phys))
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - data_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, len,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0162, VIRTIO_PCI_DEVICE_BLK, test_blk_read_huge_len_past_ram,
              "Blk read writable data len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "5.2.6");
