/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0024: virtio_mem response writable len past end of guest RAM.
 *
 * Same shape as RNG0004: program a writable response descriptor
 * whose base lives in valid guest RAM but whose length crosses the
 * end of all System RAM. The device must not access memory outside
 * the guest's mapping or crash the VMM.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_huge_len_past_ram(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_MEM_REQ_STATE;
    req->addr = 0;
    req->nb_blocks = 1;

    uint64_t resp_phys;
    if (!vv_alloc_page_near_ram_top(ram_top, &resp_phys))
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - resp_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, len,
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0024, VIRTIO_PCI_DEVICE_MEM, test_mem_huge_len_past_ram,
              "Mem response writable len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "5.15.6");
