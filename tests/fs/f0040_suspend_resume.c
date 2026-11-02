/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0040: suspend/resume of the FUSE control path.
 *
 * v1.4 3.1.4.7 suspend semantics. The orchestrator drives the
 * actual hypervisor suspend; this guest side test verifies
 * a FUSE init completes (baseline).
 */
#include "lib/virtio_test.h"
#include "lib/fuse.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    uint8_t *p = vv_alloc_pages(1);
    uint8_t *o = vv_alloc_pages(1);
    struct fuse_in_header *h = (void *)p;
    struct fuse_init_in   *i = (void *)(p + sizeof(*h));
    h->len = sizeof(*h) + sizeof(*i);
    h->opcode = FUSE_INIT; h->unique = 1;
    i->major = 7; i->minor = 31; i->max_readahead = 4096;

    vv_desc(vr, 0, p, h->len, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, o, 4096, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0040, VIRTIO_PCI_DEVICE_FS, test,
                "FUSE init baseline (suspend orchestrator companion)",
                VIRTIO_SPEC_V1_4, "3.1.4.7", 1);
