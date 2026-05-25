/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0030: FUSE LOOKUP with a zero length name.
 *
 * Spec 5.11.6: Submit a LOOKUP whose header length covers only
 * the fuse_in_header itself, leaving no bytes for the name string.
 * The daemon must reject with ENOENT or EINVAL and must not read
 * past the end of the request descriptor.
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

static test_result_t test_fs_lookup_zero_name(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* Phase 1: FUSE_INIT to bring the daemon online */
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

    /* Phase 2: LOOKUP with empty name (header only, no trailing string) */
    uint8_t *p2 = vv_alloc_pages(1);
    memset(p2, 0, 4096);
    struct fuse_in_header *hdr2 = (struct fuse_in_header *)p2;
    hdr2->len    = sizeof(*hdr2); /* no name follows */
    hdr2->opcode = FUSE_LOOKUP;
    hdr2->unique = 100;
    hdr2->nodeid = 1;

    uint64_t p2_phys = vv_virt_to_phys(p2);

    vring_raw_set_desc(vr, 2, p2_phys, hdr2->len, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, p2_phys + 256, 256, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0030, VIRTIO_PCI_DEVICE_FS, test_fs_lookup_zero_name,
                "FUSE LOOKUP with zero length name",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
