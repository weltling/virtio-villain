/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0036: NOTIFICATION_DATA feature acceptance on virtiofs.
 *
 * v1.4 5.11.4 plus VIRTIO_F_NOTIFICATION_DATA (bit 38). When
 * negotiated the driver writes additional data in the kick.
 * Verify the device offers it and accepts negotiation; then
 * send a normal init request to verify the data path still
 * works.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>

#define VIRTIO_F_NOTIFICATION_DATA 38

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
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << (VIRTIO_F_NOTIFICATION_DATA - 32))))
        return TEST_SKIP;

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

REGISTER_TEST_Q(F0036, VIRTIO_PCI_DEVICE_FS, test,
                "FUSE init with NOTIFICATION_DATA offered",
                VIRTIO_SPEC_V1_4, "5.11.4", 1);
