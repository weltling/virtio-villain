/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0111: blk_read_after_discard
 *
 * Discard a sector range then immediately read from the same range.
 * Tests discard+read interaction - the device must handle a read
 * to discarded sectors gracefully (returning zeroes or stale data).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_read_after_discard(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    /* --- Discard request: sector 0, 8 sectors --- */
    struct virtio_blk_outhdr *dhdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *dstatus = vv_alloc_pages(1);

    dhdr->type = VIRTIO_BLK_T_DISCARD;
    dhdr->ioprio = 0;
    dhdr->sector = 0;

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = 0;

    *dstatus = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dhdr), sizeof(*dhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(dstatus), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* --- Read request: sector 0, 512 bytes --- */
    struct virtio_blk_outhdr *rhdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *rstatus = vv_alloc_pages(1);

    rhdr->type = VIRTIO_BLK_T_IN;
    rhdr->ioprio = 0;
    rhdr->sector = 0;
    *rstatus = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rhdr), sizeof(*rhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(rstatus), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0111, VIRTIO_PCI_DEVICE_BLK, test_blk_read_after_discard,
              "Read from sector range after discard",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
