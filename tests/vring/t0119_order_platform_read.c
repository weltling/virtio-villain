/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0119: order_platform_read_completes
 *
 * Negotiate VIRTIO_F_ORDER_PLATFORM (bit 36) and verify a normal
 * block read still completes. Spec chapter 6 (Reserved Feature Bits):
 * a driver SHOULD accept ORDER_PLATFORM when offered, and once
 * negotiated it MUST use platform memory barriers around ring
 * accesses. The harness already issues full barriers, so with the
 * feature negotiated a single read must be consumed and completed
 * with status OK. Skips when the device does not offer the feature.
 */
#include "lib/virtio_test.h"

static test_result_t test_order_platform_read_completes(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_ORDER_PLATFORM))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xff;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    if (*status != VIRTIO_BLK_S_OK)
        TFAIL("read status 0x%02x with ORDER_PLATFORM negotiated", *status);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(T0119, VIRTIO_PCI_DEVICE_BLK,
                       test_order_platform_read_completes,
                       "read completes with ORDER_PLATFORM negotiated",
                       VIRTIO_SPEC_V1_2, "6",
                       (1ULL << VIRTIO_F_ORDER_PLATFORM), 0);
