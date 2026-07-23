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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_order_platform_read_completes(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_ORDER_PLATFORM))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xff;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, vr->queue, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*status != VIRTIO_BLK_S_OK)
        TFAIL("read status 0x%02x with ORDER_PLATFORM negotiated", *status);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(T0119, VIRTIO_PCI_DEVICE_BLK,
                       test_order_platform_read_completes,
                       "read completes with ORDER_PLATFORM negotiated",
                       VIRTIO_SPEC_V1_2, "6",
                       (1ULL << VIRTIO_F_ORDER_PLATFORM), 0);
