/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0181: discard then read does not crash.
 *
 * Spec 5.2.6.2: After DISCARD the content of the affected range
 * is undefined, but the device must not crash or return stale
 * data from a freed backing page. Submit DISCARD on sectors 8..15,
 * then READ sector 8 and accept any data without asserting content.
 * The value is that the device survives the sequence.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_discard_then_read_safe(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_DISCARD)))
        return TEST_SKIP;

    /* DISCARD sectors 8..15 */
    struct virtio_blk_outhdr *dhdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);

    dhdr->type = VIRTIO_BLK_T_DISCARD;
    dhdr->ioprio = 0;
    dhdr->sector = 0;
    seg->sector = 8;
    seg->num_sectors = 8;
    seg->flags = 0;
    *dst = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dhdr), sizeof(*dhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(dst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*dst != VIRTIO_BLK_S_OK) TFAIL("discard status %u", *dst);

    /* READ sector 8, accept any content */
    struct virtio_blk_outhdr *rhdr = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    rhdr->type = VIRTIO_BLK_T_IN;
    rhdr->ioprio = 0;
    rhdr->sector = 8;
    *rst = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(rhdr), sizeof(*rhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(rdata), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(rst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*rst != VIRTIO_BLK_S_OK) TFAIL("read status %u", *rst);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0181, VIRTIO_PCI_DEVICE_BLK,
              test_blk_discard_then_read_safe,
              "Discard then read the same range survives",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_DISCARD), 0);
