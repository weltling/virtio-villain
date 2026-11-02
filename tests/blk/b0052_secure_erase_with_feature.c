/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0052: secure_erase_with_feature
 *
 * Negotiate VIRTIO_BLK_F_SECURE_ERASE and submit a secure erase
 * command for sector 0. Verify the device completes successfully.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_secure_erase(struct virtio_dev *dev,
                                           struct vring *vr)
{
    /* Check if device offers SECURE_ERASE (feature bit 16, word 0) */
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_SECURE_ERASE))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_SECURE_ERASE, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = 8; /* erase 8 sectors */
    seg->flags = 0;

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(B0052, VIRTIO_PCI_DEVICE_BLK, test_blk_secure_erase,
              "Secure erase with VIRTIO_BLK_F_SECURE_ERASE negotiated",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_SECURE_ERASE), 0);
