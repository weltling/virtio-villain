/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0009: virtio-fs FUSE INTERRUPT on the hiprio queue.
 *
 * The hiprio queue (queue index 0) carries FUSE FORGET, INTERRUPT
 * and BATCH FORGET messages. Submit a FUSE INTERRUPT for a
 * unique that was never sent. The device and daemon must handle
 * the unknown unique cleanly.
 */
#include "lib/virtio_test.h"
#include "lib/fuse.h"

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

    vv_desc(vr, 0, page, hdr->len, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(F0009, VIRTIO_PCI_DEVICE_FS, test_fs_hiprio_interrupt,
              "FS hiprio FUSE INTERRUPT for unknown unique",
              VIRTIO_SPEC_V1_2, "5.11.6.1");
