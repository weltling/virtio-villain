/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0001: virtio-fs FUSE INIT on the request queue.
 *
 * Submit a minimal FUSE INIT request on the first request queue
 * (queue index 1, queue 0 is hiprio per spec 5.11). The chain is
 * a read only fuse_in_header + fuse_init_in followed by a
 * writable fuse_out_header + fuse_init_out. The device must
 * forward the message and the daemon must respond.
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

static test_result_t test_fs_init(struct virtio_dev *dev,
                                  struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    struct fuse_in_header *hdr = (struct fuse_in_header *)page;
    struct fuse_init_in *init = (struct fuse_init_in *)(page + sizeof(*hdr));
    uint8_t *resp = page + 256;

    memset(page, 0, 4096);
    hdr->len    = sizeof(*hdr) + sizeof(*init);
    hdr->opcode = FUSE_INIT;
    hdr->unique = 1;
    init->major = 7;
    init->minor = 31;
    init->max_readahead = 4096;
    init->flags = 0;

    uint64_t hdr_phys  = vv_virt_to_phys(page);
    uint64_t resp_phys = vv_virt_to_phys(resp);

    vring_raw_set_desc(vr, 0, hdr_phys, hdr->len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, 256, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0001, VIRTIO_PCI_DEVICE_FS, test_fs_init,
                "FUSE INIT on request queue",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
