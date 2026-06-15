/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0212: blk_only_head_descriptor
 *
 * Submit a request that consists of the header descriptor alone: a
 * single readable 16 byte descriptor with no VRING_DESC_F_NEXT, so the
 * chain carries no data descriptor and no status descriptor. A virtio
 * blk request always needs at least the status byte descriptor, so the
 * device must decline the request without crashing or wedging.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_only_head_descriptor(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0x7fffffff;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);

    /* Lone header descriptor: readable, no NEXT, no data, no status. */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_expect_reject(dev, vr, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0212, VIRTIO_PCI_DEVICE_BLK, test_blk_only_head_descriptor,
              "Request with only the head descriptor and no status",
              VIRTIO_SPEC_V1_2, "5.2.6");
