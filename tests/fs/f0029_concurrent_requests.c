/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0029: FUSE concurrent requests on requestq.
 *
 * Spec 5.11.6: After FUSE_INIT, submit two LOOKUP requests in
 * the same avail ring batch. The daemon must handle concurrent
 * in flight requests without confusion or crash.
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

static test_result_t test_fs_concurrent_requests(struct virtio_dev *dev,
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

    /* Phase 2: Two LOOKUPs submitted in one batch */
    uint8_t *p2 = vv_alloc_pages(1);
    uint8_t *p3 = vv_alloc_pages(1);
    memset(p2, 0, 4096);
    memset(p3, 0, 4096);

    /* Request A */
    struct fuse_in_header *hdrA = (struct fuse_in_header *)p2;
    char *nameA = (char *)(p2 + sizeof(*hdrA));
    memcpy(nameA, "aaa\0", 4);
    hdrA->len    = sizeof(*hdrA) + 4;
    hdrA->opcode = FUSE_LOOKUP;
    hdrA->unique = 100;
    hdrA->nodeid = 1;

    /* Request B */
    struct fuse_in_header *hdrB = (struct fuse_in_header *)p3;
    char *nameB = (char *)(p3 + sizeof(*hdrB));
    memcpy(nameB, "bbb\0", 4);
    hdrB->len    = sizeof(*hdrB) + 4;
    hdrB->opcode = FUSE_LOOKUP;
    hdrB->unique = 101;
    hdrB->nodeid = 1;

    uint64_t pA = vv_virt_to_phys(p2);
    uint64_t pB = vv_virt_to_phys(p3);

    /* Chain A: desc 2 -> desc 3 */
    vring_raw_set_desc(vr, 2, pA, hdrA->len, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, pA + 256, 256, VRING_DESC_F_WRITE, 0);

    /* Chain B: desc 4 -> desc 5 */
    vring_raw_set_desc(vr, 4, pB, hdrB->len, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, pB + 256, 256, VRING_DESC_F_WRITE, 0);

    /* Publish both at once */
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0029, VIRTIO_PCI_DEVICE_FS, test_fs_concurrent_requests,
                "Two FUSE LOOKUPs submitted concurrently",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
