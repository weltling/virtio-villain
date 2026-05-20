/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0021: FUSE BATCH_FORGET with zero count.
 *
 * Spec 5.11.6: Submit a BATCH_FORGET (opcode 42) with count=0.
 * The daemon must handle this degenerate case without crashing
 * or looping.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define FUSE_INIT         26
#define FUSE_BATCH_FORGET 42

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

struct fuse_batch_forget_in {
    uint32_t count;
    uint32_t dummy;
} __attribute__((packed));

static test_result_t test_fs_batch_forget_zero(struct virtio_dev *dev,
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

    /* Phase 2: BATCH_FORGET with count=0 */
    uint8_t *page2 = vv_alloc_pages(1);
    memset(page2, 0, 4096);

    struct fuse_in_header *bhdr = (struct fuse_in_header *)page2;
    struct fuse_batch_forget_in *bf =
        (struct fuse_batch_forget_in *)(page2 + sizeof(*bhdr));

    bhdr->len    = sizeof(*bhdr) + sizeof(*bf);
    bhdr->opcode = FUSE_BATCH_FORGET;
    bhdr->unique = 0; /* FORGET has no reply */
    bhdr->nodeid = 0;
    bf->count    = 0;

    uint64_t phys2 = vv_virt_to_phys(page2);

    /* BATCH_FORGET expects no response */
    vring_raw_set_desc(vr, 2, phys2, bhdr->len, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0021, VIRTIO_PCI_DEVICE_FS, test_fs_batch_forget_zero,
                "FUSE BATCH_FORGET with zero count",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
