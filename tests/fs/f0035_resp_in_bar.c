/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0035: FUSE response descriptor in device MMIO BAR.
 *
 * Spec 5.11.6: A FUSE request is a readable header plus a
 * writable response. Submit FUSE_INIT whose writable response
 * descriptor addr points at the device's own common
 * configuration BAR rather than RAM. A daemon that writes the
 * reply through the generic memory API without validating the
 * target region can wedge or corrupt the device's own
 * registers. The device must reject or handle the non RAM
 * target cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

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

static test_result_t test_fs_resp_in_bar(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *p = vv_alloc_pages(1);
    memset(p, 0, 4096);
    struct fuse_in_header *h = (struct fuse_in_header *)p;
    struct fuse_init_in   *i = (struct fuse_init_in *)(p + sizeof(*h));
    h->len    = sizeof(*h) + sizeof(*i);
    h->opcode = FUSE_INIT;
    h->unique = 1;
    i->major  = 7;
    i->minor  = 31;
    i->max_readahead = 4096;

    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(p), h->len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, mmio_phys, 256, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0035, VIRTIO_PCI_DEVICE_FS, test_fs_resp_in_bar,
                "FUSE response descriptor in device MMIO BAR",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
