/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0024: FUSE request with wrong descriptor order.
 *
 * Spec 5.11.6: Submit a FUSE request where the writable response
 * descriptor appears before the readable request descriptor in the
 * chain. The device must reject or handle the malformed chain
 * gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define FUSE_INIT 26

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

static test_result_t test_fs_wrong_desc_order(struct virtio_dev *dev,
                                              struct vring *vr)
{
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

    /* Wrong order: writable first, then readable */
    vring_raw_set_desc(vr, 0, phys + 256, 256,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys, hdr->len, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0024, VIRTIO_PCI_DEVICE_FS, test_fs_wrong_desc_order,
                "FUSE request with writable before readable descriptor",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
