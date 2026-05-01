/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0013: desc_no_status
 *
 * Submit a request with a header and data descriptor but no status
 * descriptor. The spec requires a trailing 1-byte writable descriptor
 * for status. A VMM that blindly expects the third descriptor may
 * read past the chain or crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_desc_no_status(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    /*
     * Descriptor chain: header -> data, NO status descriptor.
     * The chain ends at [1] (no NEXT flag).
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_WRITE, 0); /* no NEXT -> chain ends */

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0013, VIRTIO_PCI_DEVICE_BLK, test_desc_no_status,
              "Chain with no status descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
