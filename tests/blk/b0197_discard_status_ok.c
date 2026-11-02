/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0197: discard returns S_OK for valid range.
 *
 * Submit DISCARD for sectors 0..7 and verify the device writes
 * S_OK in the status byte. Positive discard path test.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_ok(struct virtio_dev *dev,
                                         struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_DISCARD))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_DISCARD; hdr->ioprio = 0; hdr->sector = 0;
    seg->sector = 0; seg->num_sectors = 8; seg->flags = 0;
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("discard status %u, expected S_OK", *st);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0197, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_ok,
              "Discard valid range returns S_OK",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_DISCARD), 0);
