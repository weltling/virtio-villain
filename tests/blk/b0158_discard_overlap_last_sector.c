/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0158: blk_discard_overlap_last_sector
 *
 * Submit a DISCARD whose first segment reaches one sector past
 * the last valid sector of the device. The device must reject
 * the segment, leave capacity intact, and stay alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_discard_overlap_last(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    /* Read capacity from device_cfg first u64 */
    volatile uint64_t *capacity_p = (volatile uint64_t *)dev->device_cfg;
    __sync_synchronize();
    uint64_t capacity = *capacity_p;
    if (capacity == 0)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_DISCARD;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Start two sectors before the end, length four sectors */
    seg->sector = capacity - 2;
    seg->num_sectors = 4;
    seg->flags = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS && r != TEST_REJECT)
        return r;

    /* Capacity unchanged */
    __sync_synchronize();
    if (*capacity_p != capacity)
        TFAIL("*capacity_p != capacity");

    return r;
}

REGISTER_TEST(B0158, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_overlap_last,
              "DISCARD segment straddling the last sector",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
