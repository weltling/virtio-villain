/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0017: FUSE LOOKUP with invalid nodeid.
 *
 * Spec 5.11.6: After establishing the FUSE session, submit a FUSE
 * LOOKUP request with nodeid set to UINT64_MAX, referencing a parent
 * inode that has never existed. The daemon must return an error
 * (e.g. ENOENT) or drop the request without crashing.
 * Adapted from QEMU virtio-9p-test walk_nonexistent pattern.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define FUSE_INIT   26
#define FUSE_LOOKUP 1

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

struct fuse_init_in {
    uint32_t major;
    uint32_t minor;
    uint32_t max_readahead;
    uint32_t flags;
} __attribute__((packed));

static test_result_t test_fs_lookup_invalid_nodeid(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    /* Phase 1: FUSE_INIT to establish the session */
    struct fuse_in_header *hdr = (struct fuse_in_header *)page;
    struct fuse_init_in *init = (struct fuse_init_in *)(page + sizeof(*hdr));

    hdr->len    = sizeof(*hdr) + sizeof(*init);
    hdr->opcode = FUSE_INIT;
    hdr->unique = 1;
    init->major = 7;
    init->minor = 31;
    init->max_readahead = 4096;

    uint64_t phys = vv_virt_to_phys(page);
    uint8_t *resp = page + 256;
    uint64_t resp_phys = vv_virt_to_phys(resp);

    vring_raw_set_desc(vr, 0, phys, hdr->len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, 256, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Phase 2: FUSE_LOOKUP with invalid parent nodeid */
    uint8_t *page2 = vv_alloc_pages(1);
    memset(page2, 0, 4096);

    struct fuse_in_header *lhdr = (struct fuse_in_header *)page2;
    char *name = (char *)(page2 + sizeof(*lhdr));

    memcpy(name, "nonexistent\0", 12);
    lhdr->len    = sizeof(*lhdr) + 12;
    lhdr->opcode = FUSE_LOOKUP;
    lhdr->unique = 100;
    lhdr->nodeid = 0xFFFFFFFFFFFFFFFFULL; /* invalid parent inode */

    uint64_t phys2 = vv_virt_to_phys(page2);

    vring_raw_set_desc(vr, 2, phys2, lhdr->len, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, phys2 + 256, 256, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0017, VIRTIO_PCI_DEVICE_FS, test_fs_lookup_invalid_nodeid,
                "FUSE LOOKUP with invalid parent nodeid",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
