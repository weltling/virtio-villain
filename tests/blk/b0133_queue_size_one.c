/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0133: Queue size=1 with full request chain.
 *
 * Attempt to submit a full block request (header + data + status = 3
 * descriptors) on a queue that was configured with size 1. The device
 * cannot complete this chain.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_queue_size_one(struct virtio_dev *dev,
                                             struct vring *vr)
{
    /*
     * Configure a queue of size 1 by writing queue_size=1 via MMIO/PCI.
     * This is only valid if the device supports queue_size negotiation.
     * With queue_size=1, only descriptor index 0 exists.
     */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Post 3 descriptors even though ring might only have 1 slot.
     * The device must detect the overflow or invalid next index.
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0133, VIRTIO_PCI_DEVICE_BLK, test_blk_queue_size_one,
              "Full request chain on queue with size=1",
              VIRTIO_SPEC_V1_2, "5.2.6");
