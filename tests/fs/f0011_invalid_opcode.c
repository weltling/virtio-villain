/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0011: FUSE request with invalid opcode.
 *
 * Spec 5.11.6: Submit a FUSE request whose opcode is set to a
 * value beyond any defined FUSE operation. The virtiofs device
 * (or the backing daemon) must return ENOSYS without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_invalid_opcode(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct fuse_in_header *hdr = (struct fuse_in_header *)page;
    hdr->len = sizeof(*hdr);
    hdr->opcode = 0xFFFF; /* invalid */
    hdr->unique = 42;

    uint64_t hdr_phys = vv_virt_to_phys(page);
    uint64_t resp_phys = hdr_phys + 256;

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, 256,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0011, VIRTIO_PCI_DEVICE_FS, test_fs_invalid_opcode,
                "FUSE request with invalid opcode",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
