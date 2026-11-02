/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0189: flush returns S_OK status.
 *
 * Submit a FLUSH request and verify the device writes S_OK (0) in the
 * status byte. Existing flush tests only check the used ring advances;
 * this test explicitly validates the status field value.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_flush_status(struct virtio_dev *dev,
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
        TFAIL("flush status %u, expected S_OK (0)", *st);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0189, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_status,
              "Flush returns S_OK status",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_FLUSH), 0);
