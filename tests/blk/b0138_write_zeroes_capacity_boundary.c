/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0138: Write-zeroes spanning last sector (capacity boundary).
 *
 * Submit a WRITE_ZEROES request where sector + num_sectors reaches
 * exactly the device capacity boundary.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_write_zeroes_capacity_boundary(
    struct virtio_dev *dev, struct vring *vr)
{
    /* Read capacity from config space */
    volatile uint8_t *cfg_space = (volatile uint8_t *)dev->device_cfg;
    uint64_t capacity;
    memcpy(&capacity, (void *)cfg_space, sizeof(capacity));

    if (capacity < 2)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Request from last sector to end of device */
    seg->sector = capacity - 1;
    seg->num_sectors = 1;
    seg->flags = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t seg_phys = vv_virt_to_phys(seg);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, seg_phys, sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0138, VIRTIO_PCI_DEVICE_BLK, test_blk_write_zeroes_capacity_boundary,
              "Write-zeroes spanning last sector at capacity boundary",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
