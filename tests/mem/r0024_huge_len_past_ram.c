/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0024: virtio_mem response writable len past end of guest RAM.
 *
 * Same shape as RNG0004: program a writable response descriptor
 * whose base lives in valid guest RAM but whose length crosses the
 * end of all System RAM. The device must not access memory outside
 * the guest's mapping or crash the VMM.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static test_result_t test_mem_huge_len_past_ram(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_STATE;
    req->addr = 0;
    req->nb_blocks = 1;

    uint64_t req_phys = vv_virt_to_phys(req);

    uint8_t *resp_buf = vv_alloc_pages(1);
    uint64_t resp_phys = vv_virt_to_phys(resp_buf);
    if (resp_phys >= ram_top)
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - resp_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, req_phys, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, len,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0024, VIRTIO_PCI_DEVICE_MEM, test_mem_huge_len_past_ram,
              "Mem response writable len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "5.15.6");
