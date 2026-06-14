/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0033: FUSE READ with size = 0 after INIT.
 *
 * Spec 5.11.6: FUSE READ carries a 32 bit size for the requested
 * payload. Submit FUSE_READ with size=0 on the root inode. The
 * daemon must return a zero length response cleanly rather than
 * treating the zero as "unlimited" or as a malformed request.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_read_zero_size(struct virtio_dev *dev,
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
    struct fuse_read_in   *ri = (struct fuse_read_in *)(p2 + sizeof(*h2));
    h2->len    = sizeof(*h2) + sizeof(*ri);
    h2->opcode = FUSE_READ;
    h2->unique = 9;
    h2->nodeid = 1;
    ri->fh     = 0;
    ri->offset = 0;
    ri->size   = 0;

    uint64_t p2_phys = vv_virt_to_phys(p2);
    vring_raw_set_desc(vr, 2, p2_phys, h2->len, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, p2_phys + 512, 512, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0033, VIRTIO_PCI_DEVICE_FS, test_fs_read_zero_size,
                "FUSE READ with size=0 on root inode",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
