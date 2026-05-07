/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0009: virtio-fs FUSE INTERRUPT on the hiprio queue.
 *
 * The hiprio queue (queue index 0) carries FUSE FORGET, INTERRUPT
 * and BATCH FORGET messages. Submit a FUSE INTERRUPT for a
 * unique that was never sent. The device and daemon must handle
 * the unknown unique cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define FUSE_INTERRUPT 36

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

struct fuse_interrupt_in {
    uint64_t unique;
} __attribute__((packed));

static test_result_t test_fs_hiprio_interrupt(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    struct fuse_in_header_min *hdr = (struct fuse_in_header_min *)page;
    struct fuse_interrupt_in *intr =
        (struct fuse_interrupt_in *)(page + sizeof(*hdr));

    memset(page, 0, 4096);
    hdr->len    = sizeof(*hdr) + sizeof(*intr);
    hdr->opcode = FUSE_INTERRUPT;
    hdr->unique = 0;
    intr->unique = 0xDEADBEEFCAFEBABEULL;

    uint64_t hdr_phys = vv_virt_to_phys(page);

    vring_raw_set_desc(vr, 0, hdr_phys, hdr->len, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(F0009, VIRTIO_PCI_DEVICE_FS, test_fs_hiprio_interrupt,
              "FS hiprio FUSE INTERRUPT for unknown unique",
              VIRTIO_SPEC_V1_2, "5.11.6.1");
