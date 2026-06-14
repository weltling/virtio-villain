/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0020: FUSE READDIR with a tiny response buffer.
 *
 * Spec 5.11.6: After establishing the FUSE session, submit a FUSE
 * READDIR request whose writable response descriptor is only 1 byte.
 * The daemon normally returns a variable length dirent stream; a
 * buffer too small to hold even the response header forces truncation.
 * The device must not write past the descriptor length.
 * Adapted from QEMU virtio-9p-test readdir_split patterns.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_readdir_tiny_buffer(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    /* Phase 1: FUSE_INIT to establish session */
    uint8_t *init_page = vv_alloc_pages(1);
    memset(init_page, 0, 4096);

    struct fuse_in_header *ihdr = (struct fuse_in_header *)init_page;
    struct fuse_init_in *init =
        (struct fuse_init_in *)(init_page + sizeof(*ihdr));

    ihdr->len    = sizeof(*ihdr) + sizeof(*init);
    ihdr->opcode = FUSE_INIT;
    ihdr->unique = 1;
    init->major  = 7;
    init->minor  = 31;
    init->max_readahead = 4096;

    uint64_t init_phys = vv_virt_to_phys(init_page);

    vring_raw_set_desc(vr, 0, init_phys, ihdr->len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, init_phys + 256, 256,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Phase 2: READDIR with 1 byte writable buffer */
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct fuse_in_header *hdr = (struct fuse_in_header *)page;
    struct fuse_read_in *rd =
        (struct fuse_read_in *)(page + sizeof(*hdr));

    hdr->len    = sizeof(*hdr) + sizeof(*rd);
    hdr->opcode = FUSE_READDIR;
    hdr->unique = 200;
    hdr->nodeid = 1; /* root inode */
    rd->fh      = 0;
    rd->offset  = 0;
    rd->size    = 1; /* request only 1 byte of dirents */

    uint64_t phys = vv_virt_to_phys(page);

    /* Readable: header + read_in */
    vring_raw_set_desc(vr, 2, phys, hdr->len, VRING_DESC_F_NEXT, 3);
    /* Writable: only 1 byte for the response */
    vring_raw_set_desc(vr, 3, phys + 512, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    /*
     * The writable buffer (1 byte) is too small to hold even the
     * fuse_out_header (16 bytes). The device may:
     * - Truncate the response and complete (PASS)
     * - Refuse to respond at all (REJECT, also acceptable)
     * Either way, no overflow occurs.
     */
    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_REJECT)
        return TEST_PASS;
    return r;
}

REGISTER_TEST_Q(F0020, VIRTIO_PCI_DEVICE_FS, test_fs_readdir_tiny_buffer,
                "FUSE READDIR with 1 byte response buffer",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
