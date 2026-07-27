/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0016: virtio-fs response writable len past end of guest RAM.
 *
 * Same shape as RNG0004: submit a FUSE INIT chain whose response
 * descriptor base lives in valid guest RAM but whose length
 * crosses the end of all System RAM. Device must not access
 * memory outside the guest's mapping or crash the VMM.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static test_result_t test_fs_resp_huge_len_past_ram(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    uint8_t *page = vv_alloc_pages(1);
    struct fuse_in_header_min *hdr = (struct fuse_in_header_min *)page;
    memset(page, 0, 4096);
    hdr->len    = sizeof(*hdr) + 16;
    hdr->opcode = FUSE_INIT;
    hdr->unique = 1;

    uint64_t hdr_phys  = vv_virt_to_phys(page);
    uint64_t resp_phys;
    if (!vv_alloc_page_near_ram_top(ram_top, &resp_phys))
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - resp_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, hdr_phys, hdr->len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, len, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0016, VIRTIO_PCI_DEVICE_FS, test_fs_resp_huge_len_past_ram,
                "FS response writable len crosses end of RAM",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
