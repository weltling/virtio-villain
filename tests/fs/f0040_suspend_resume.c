/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0040: suspend/resume of the FUSE control path.
 *
 * v1.4 3.1.4.7 suspend semantics. The orchestrator drives the
 * actual hypervisor suspend; this guest side test verifies
 * a FUSE init completes (baseline).
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>

#define FUSE_INIT 26
struct fuse_in_header {
    uint32_t len; uint32_t opcode; uint64_t unique; uint64_t nodeid;
    uint32_t uid; uint32_t gid; uint32_t pid; uint32_t padding;
} __attribute__((packed));
struct fuse_init_in {
    uint32_t major; uint32_t minor; uint32_t max_readahead; uint32_t flags;
} __attribute__((packed));

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    uint8_t *p = vv_alloc_pages(1);
    uint8_t *o = vv_alloc_pages(1);
    memset(p, 0, 4096);
    struct fuse_in_header *h = (void *)p;
    struct fuse_init_in   *i = (void *)(p + sizeof(*h));
    h->len = sizeof(*h) + sizeof(*i);
    h->opcode = FUSE_INIT; h->unique = 1;
    i->major = 7; i->minor = 31; i->max_readahead = 4096;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(p), h->len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(o), 4096,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0040, VIRTIO_PCI_DEVICE_FS, test,
                "FUSE init baseline (suspend orchestrator companion)",
                VIRTIO_SPEC_V1_4, "3.1.4.7", 1);
