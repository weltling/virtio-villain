/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0185: flush on a fresh device before any writes.
 *
 * Spec 5.2.6.2: FLUSH makes all prior writes stable. If no writes
 * have occurred, the flush should succeed trivially. The device
 * must not fail or hang on an empty flush.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_flush_empty(struct virtio_dev *dev,
                                          struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_FLUSH))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("flush status %u on fresh device", *st);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0185, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_empty,
              "Flush on fresh device before any writes",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_FLUSH), 0);
