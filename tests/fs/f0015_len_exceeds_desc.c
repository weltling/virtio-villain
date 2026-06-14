/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0015: FUSE request whose len exceeds descriptor length.
 *
 * Spec 5.11.6: The fuse_in_header.len field states the total
 * request size. Set len to 4096 but provide only 40 bytes in the
 * readable descriptor. The device must use the descriptor length
 * as the true bound and not trust the header len.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_len_exceeds_desc(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct fuse_in_header *hdr = (struct fuse_in_header *)page;
    hdr->len = 4096; /* claims big, desc is small */
    hdr->opcode = FUSE_INIT;
    hdr->unique = 7;

    uint64_t phys = vv_virt_to_phys(page);

    /* Provide only sizeof(fuse_in_header) readable bytes */
    vring_raw_set_desc(vr, 0, phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + 512, 256,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0015, VIRTIO_PCI_DEVICE_FS, test_fs_len_exceeds_desc,
                "FUSE header len exceeds descriptor length",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
