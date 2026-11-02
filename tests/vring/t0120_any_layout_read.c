/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0120: any_layout_read_completes
 *
 * Negotiate VIRTIO_F_ANY_LAYOUT (bit 27) and verify a normal block
 * read still completes. Spec 2.7.4.1: a device MUST NOT make
 * assumptions about the particular arrangement of descriptors, which
 * is exactly what ANY_LAYOUT expresses for a transitional device.
 * With the feature negotiated a single three descriptor read must be
 * consumed and completed with status OK. Skips when the device does
 * not offer the feature.
 */
#include "lib/virtio_test.h"

static test_result_t test_any_layout_read_completes(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_ANY_LAYOUT))
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
        TFAIL("read status 0x%02x with ANY_LAYOUT negotiated", *status);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(T0120, VIRTIO_PCI_DEVICE_BLK,
                       test_any_layout_read_completes,
                       "read completes with ANY_LAYOUT negotiated",
                       VIRTIO_SPEC_V1_2, "2.7.4.1",
                       (1ULL << VIRTIO_F_ANY_LAYOUT), 0);
