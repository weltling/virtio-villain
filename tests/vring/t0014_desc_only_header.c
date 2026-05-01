/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0014: desc_only_header
 *
 * Submit a request with only the header descriptor - no data, no status.
 * This is an even more truncated chain than T13.
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

static test_result_t test_desc_only_header(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);

    /*
     * Single descriptor: just the header, no NEXT flag.
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0014, VIRTIO_PCI_DEVICE_BLK, test_desc_only_header,
              "Chain with only header descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
