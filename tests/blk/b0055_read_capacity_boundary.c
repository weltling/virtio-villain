/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0055: read_at_capacity_boundary
 *
 * Read exactly the last valid sector (capacity - 1). This should
 * succeed since the request is entirely in-bounds.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_read_capacity_boundary(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile uint64_t *cap = (volatile uint64_t *)dev->device_cfg;
    uint64_t capacity = *cap;

    if (capacity < 1)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = capacity - 1; /* exactly the last sector */
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0055, VIRTIO_PCI_DEVICE_BLK, test_blk_read_capacity_boundary,
              "Read exactly at last valid sector (capacity - 1)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
