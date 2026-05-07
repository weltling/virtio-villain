/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0003: virtio-fs request with non writable response descriptor.
 *
 * Submit a FUSE INIT chain whose response slot lacks the WRITE
 * flag. The device must reject the chain rather than write to
 * a read only buffer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define FUSE_INIT 26

struct fuse_in_header_min {
    uint32_t len;
    uint32_t opcode;
    uint64_t unique;
    uint64_t nodeid;
    uint32_t uid;
    uint32_t gid;
    uint32_t pid;
    uint32_t padding;
} __attribute__((packed));

static test_result_t test_fs_resp_no_write(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    struct fuse_in_header_min *hdr = (struct fuse_in_header_min *)page;
    memset(page, 0, 4096);
    hdr->len    = sizeof(*hdr) + 16;
    hdr->opcode = FUSE_INIT;
    hdr->unique = 1;

    uint64_t hdr_phys  = vv_virt_to_phys(page);
    uint64_t resp_phys = hdr_phys + 256;

    vring_raw_set_desc(vr, 0, hdr_phys, hdr->len, VRING_DESC_F_NEXT, 1);
    /* Response slot without WRITE - device must reject. */
    vring_raw_set_desc(vr, 1, resp_phys, 256, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0003, VIRTIO_PCI_DEVICE_FS, test_fs_resp_no_write,
                "FS response without WRITE flag",
                VIRTIO_SPEC_V1_2, "2.7.5.3", 1);
