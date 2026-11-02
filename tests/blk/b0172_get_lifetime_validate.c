/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0172: get_lifetime_validate_fields
 *
 * Spec 5.2.6.2: the GET_LIFETIME response has pre_eol_info (0..3),
 * device_lifetime_est_typ_a (0..11), and device_lifetime_est_typ_b
 * (0..11). If the device supports VIRTIO_BLK_F_LIFETIME and returns
 * S_OK, verify the response fields are within these spec ranges.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_lifetime_fields(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_LIFETIME))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_LIFETIME, 0);
    struct virtio_blk_lifetime *life = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    memset(life, 0xFF, sizeof(*life));
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, life, sizeof(*life), VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
            2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    if (*st == VIRTIO_BLK_S_UNSUPP)
        return TEST_SKIP;
    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("status %u", *st);

    /* Validate ranges per spec */
    if (life->pre_eol_info > 3)
        TFAIL("pre_eol_info %u > 3", life->pre_eol_info);
    if (life->device_lifetime_est_typ_a > 11)
        TFAIL("lifetime_est_a %u > 11", life->device_lifetime_est_typ_a);
    if (life->device_lifetime_est_typ_b > 11)
        TFAIL("lifetime_est_b %u > 11", life->device_lifetime_est_typ_b);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0172, VIRTIO_PCI_DEVICE_BLK,
              test_blk_lifetime_fields,
              "GET_LIFETIME response fields within spec ranges",
              VIRTIO_SPEC_V1_4, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_LIFETIME), 0);
