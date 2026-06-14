/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0013: FUSE INIT with wrong major version.
 *
 * Spec 5.11.6: The first request must be FUSE_INIT. Sending it
 * with major=0 (unsupported) should cause the daemon to reject
 * the handshake. The device must relay the error safely.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_init_bad_version(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct fuse_in_header *hdr = (struct fuse_in_header *)page;
    struct fuse_init_in *init = (struct fuse_init_in *)(page + sizeof(*hdr));

    hdr->len = sizeof(*hdr) + sizeof(*init);
    hdr->opcode = FUSE_INIT;
    hdr->unique = 1;
    init->major = 0; /* invalid */
    init->minor = 0;
    init->max_readahead = 4096;
    init->flags = 0;

    uint64_t phys = vv_virt_to_phys(page);

    vring_raw_set_desc(vr, 0, phys, hdr->len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + 256, 256,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0013, VIRTIO_PCI_DEVICE_FS, test_fs_init_bad_version,
                "FUSE INIT with unsupported major version",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
