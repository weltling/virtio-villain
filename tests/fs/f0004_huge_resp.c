/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0004: virtio-fs response with oversized writable length.
 *
 * Submit a FUSE INIT chain whose response slot claims 1 GiB of
 * writable space but the underlying allocation is one page. The
 * device must clamp writes to the real buffer size or refuse the
 * descriptor; it must not write past the page.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define FUSE_INIT 26

struct fuse_in_header_min {
    uint32_t len;
    uint32_t opcode;
    uint64_t unique;
    uint64_t nodeid;
    uint32_t uid;
    uint32_t gid;
    uint32_t pid;
    uint32_t padding;
} __attribute__((packed));

static test_result_t test_fs_huge_resp(struct virtio_dev *dev,
                                       struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    struct fuse_in_header_min *hdr = (struct fuse_in_header_min *)page;
    memset(page, 0, 4096);
    hdr->len    = sizeof(*hdr) + 16;
    hdr->opcode = FUSE_INIT;
    hdr->unique = 1;

    uint64_t hdr_phys  = vv_virt_to_phys(page);
    uint64_t resp_phys = hdr_phys + 256;

    vring_raw_set_desc(vr, 0, hdr_phys, hdr->len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, 1u << 30, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0004, VIRTIO_PCI_DEVICE_FS, test_fs_huge_resp,
                "FS response with 1 GiB length",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
