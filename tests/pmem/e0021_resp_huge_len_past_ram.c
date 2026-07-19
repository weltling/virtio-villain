/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0021: pmem flush response writable len past end of guest RAM.
 *
 * Same shape as RNG0004: submit a flush whose response descriptor
 * base lives in valid guest RAM but whose length crosses the end
 * of all System RAM. Device must not access memory outside the
 * guest's mapping or crash the VMM.
 *
 * Spec 5.10.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static test_result_t test_pmem_resp_huge_len_past_ram(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    void *r = vv_alloc_pages(1);
    uint64_t r_phys = vv_virt_to_phys(r);
    if (r_phys >= ram_top)
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - r_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, r_phys, len, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0021, VIRTIO_PCI_DEVICE_PMEM, test_pmem_resp_huge_len_past_ram,
              "Pmem flush response writable len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
