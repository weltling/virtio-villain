/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0010: virtio-fs fill the request queue.
 *
 * Post a minimal FUSE INIT chain in every paired slot of the
 * request queue and advance avail->idx accordingly. The device
 * must consume the full batch without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_fill_ring(struct virtio_dev *dev,
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

    /* Each chain takes two descriptors, so we pair slots. */
    uint16_t pairs = vr->size / 2;
    for (uint16_t p = 0; p < pairs; p++) {
        uint16_t a = (uint16_t)(p * 2);
        uint16_t b = (uint16_t)(p * 2 + 1);
        vring_raw_set_desc(vr, a, hdr_phys, hdr->len,
                           VRING_DESC_F_NEXT, b);
        vring_raw_set_desc(vr, b, resp_phys, 256,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, p, a);
    }
    vring_raw_set_avail_idx(vr, pairs);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0010, VIRTIO_PCI_DEVICE_FS, test_fs_fill_ring,
                "FS fill the request ring",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
