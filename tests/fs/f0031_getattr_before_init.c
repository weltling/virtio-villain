/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0031: FUSE GETATTR submitted before FUSE_INIT.
 *
 * Spec 5.11.6 follows the FUSE wire protocol where the very first
 * request on a fresh session must be FUSE_INIT. Submit GETATTR
 * for nodeid 1 with no preceding FUSE_INIT. The daemon must
 * reject the request without acting on it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define FUSE_GETATTR 3

struct fuse_in_header {
    uint32_t len;
    uint32_t opcode;
    uint64_t unique;
    uint64_t nodeid;
    uint32_t uid;
    uint32_t gid;
    uint32_t pid;
    uint32_t padding;
} __attribute__((packed));

struct fuse_getattr_in {
    uint32_t getattr_flags;
    uint32_t dummy;
    uint64_t fh;
} __attribute__((packed));

static test_result_t test_fs_getattr_before_init(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct fuse_in_header *hdr = (struct fuse_in_header *)page;
    struct fuse_getattr_in *ga = (struct fuse_getattr_in *)(page + sizeof(*hdr));

    hdr->len    = sizeof(*hdr) + sizeof(*ga);
    hdr->opcode = FUSE_GETATTR;
    hdr->unique = 42;
    hdr->nodeid = 1;
    ga->getattr_flags = 0;
    ga->fh = 0;

    uint64_t phys = vv_virt_to_phys(page);

    vring_raw_set_desc(vr, 0, phys, hdr->len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + 512, 512, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0031, VIRTIO_PCI_DEVICE_FS, test_fs_getattr_before_init,
                "FUSE GETATTR before FUSE_INIT",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
