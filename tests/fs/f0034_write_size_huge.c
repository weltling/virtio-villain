/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0034: FUSE WRITE with size = 0xFFFFFFFF.
 *
 * Spec 5.11.6: FUSE WRITE carries a 32 bit size for the payload.
 * Submit FUSE_WRITE with size = 0xFFFFFFFF while the readable
 * descriptor only covers a few hundred bytes. A daemon that
 * trusts the declared size when reading the payload can read
 * past the end of the request buffer. The device must bound the
 * read at the available data.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define FUSE_INIT  26
#define FUSE_WRITE 16

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

struct fuse_write_in {
    uint64_t fh;
    uint64_t offset;
    uint32_t size;
    uint32_t write_flags;
    uint64_t lock_owner;
    uint32_t flags;
    uint32_t padding;
} __attribute__((packed));

static test_result_t test_fs_write_size_huge(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *p1 = vv_alloc_pages(1);
    memset(p1, 0, 4096);
    struct fuse_in_header *h1 = (struct fuse_in_header *)p1;
    struct fuse_init_in   *i1 = (struct fuse_init_in *)(p1 + sizeof(*h1));
    h1->len    = sizeof(*h1) + sizeof(*i1);
    h1->opcode = FUSE_INIT;
    h1->unique = 1;
    i1->major  = 7;
    i1->minor  = 31;
    i1->max_readahead = 4096;

    uint64_t p1_phys = vv_virt_to_phys(p1);
    vring_raw_set_desc(vr, 0, p1_phys, h1->len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, p1_phys + 512, 512, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    uint8_t *p2 = vv_alloc_pages(1);
    memset(p2, 0, 4096);
    struct fuse_in_header *h2 = (struct fuse_in_header *)p2;
    struct fuse_write_in  *wi = (struct fuse_write_in *)(p2 + sizeof(*h2));
    h2->len    = sizeof(*h2) + sizeof(*wi) + 64;
    h2->opcode = FUSE_WRITE;
    h2->unique = 11;
    h2->nodeid = 1;
    wi->fh     = 0;
    wi->offset = 0;
    wi->size   = 0xFFFFFFFFu;
    memset(p2 + sizeof(*h2) + sizeof(*wi), 'A', 64);

    uint64_t p2_phys = vv_virt_to_phys(p2);
    vring_raw_set_desc(vr, 2, p2_phys,
                       sizeof(*h2) + sizeof(*wi) + 64,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, p2_phys + 2048, 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0034, VIRTIO_PCI_DEVICE_FS, test_fs_write_size_huge,
                "FUSE WRITE with size = 0xFFFFFFFF",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
