/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0086: Two concurrent flushes in-flight
 *
 * Spec 5.2.6.2: "The device MUST ensure that all writes completed
 * before a flush request are committed."
 *
 * Submit two FLUSH requests simultaneously on the same queue to
 * stress flush ordering/completion logic.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_FLUSH 4

static test_result_t test_blk_double_flush(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *hdr2 = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_FLUSH;
    hdr1->ioprio = 0;
    hdr1->sector = 0;
    *status1 = 0xFF;

    hdr2->type = VIRTIO_BLK_T_FLUSH;
    hdr2->ioprio = 0;
    hdr2->sector = 0;
    *status2 = 0xFF;

    /* Request 1: descs 0-1 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(status1), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Request 2: descs 2-3 */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(hdr2), sizeof(*hdr2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(status2), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0086, VIRTIO_PCI_DEVICE_BLK, test_blk_double_flush,
              "Two concurrent FLUSH requests in single kick",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
