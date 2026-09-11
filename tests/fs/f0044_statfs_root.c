/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0044: FUSE STATFS on the root inode.
 *
 * Spec 5.11.6: after FUSE_INIT, submit FUSE_STATFS (opcode 17) on the
 * root inode (nodeid 1). The request carries no body. The daemon must
 * answer with a fuse_statfs_out whose error is 0 and whose block size
 * and name length are non-zero for a real filesystem. Exercises a
 * functional FUSE query path not covered elsewhere.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_statfs_root(struct virtio_dev *dev,
                                         struct vring *vr)
{
    /* Phase 1: FUSE_INIT */
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct fuse_in_header *hdr = (struct fuse_in_header *)page;
    struct fuse_init_in *init = (struct fuse_init_in *)(page + sizeof(*hdr));

    hdr->len    = sizeof(*hdr) + sizeof(*init);
    hdr->opcode = FUSE_INIT;
    hdr->unique = 1;
    init->major = 7;
    init->minor = 31;
    init->max_readahead = 4096;

    uint64_t phys = vv_virt_to_phys(page);

    vring_raw_set_desc(vr, 0, phys, hdr->len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + 256, 256, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Phase 2: FUSE_STATFS on root, no request body. */
    uint8_t *page2 = vv_alloc_pages(1);
    memset(page2, 0, 4096);

    struct fuse_in_header *shdr = (struct fuse_in_header *)page2;
    shdr->len    = sizeof(*shdr);
    shdr->opcode = FUSE_STATFS;
    shdr->unique = 200;
    shdr->nodeid = 1; /* root inode */

    uint64_t phys2 = vv_virt_to_phys(page2);
    uint8_t *resp = page2 + 256;

    vring_raw_set_desc(vr, 2, phys2, shdr->len, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, phys2 + 256,
                       sizeof(struct fuse_out_header) +
                       sizeof(struct fuse_statfs_out),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    struct fuse_out_header *ohdr = (struct fuse_out_header *)resp;
    if (ohdr->error != 0)
        TFAIL("STATFS returned error %d", ohdr->error);

    struct fuse_statfs_out *st =
        (struct fuse_statfs_out *)(resp + sizeof(*ohdr));
    if (st->st.bsize == 0)
        TFAIL("STATFS reported zero block size");
    if (st->st.namelen == 0)
        TFAIL("STATFS reported zero max name length");

    return TEST_PASS;
}

REGISTER_TEST_Q(F0044, VIRTIO_PCI_DEVICE_FS, test_fs_statfs_root,
                "FUSE STATFS on root reports a valid filesystem",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
