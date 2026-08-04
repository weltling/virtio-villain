/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0043: virtio-fs response writable addr plus len wraps 2^64.
 *
 * Siblings F0004 and F0016 keep the response end address below 2^64.
 * Here the response descriptor base sits near the top of the address
 * space and the length makes addr plus len wrap to a low value, so a
 * device that computes the end with a plain addition gets a small
 * wrapped result and a naive bounds check passes. The device must not
 * access memory outside the guest mapping or crash the VMM. Completing,
 * silently rejecting, or wedging the queue are all acceptable.
 *
 * Spec 5.11.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>

static test_result_t test_fs_resp_addr_len_wrap(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    struct fuse_in_header_min *hdr = (struct fuse_in_header_min *)page;
    memset(page, 0, 4096);
    hdr->len    = sizeof(*hdr) + 16;
    hdr->opcode = FUSE_INIT;
    hdr->unique = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(page), hdr->len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0043, VIRTIO_PCI_DEVICE_FS, test_fs_resp_addr_len_wrap,
                "FS response writable addr plus len wraps 64 bits",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
