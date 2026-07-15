/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0172: get_lifetime_validate_fields
 *
 * Spec 5.2.6.2: the GET_LIFETIME response has pre_eol_info (0..3),
 * device_lifetime_est_typ_a (0..11), and device_lifetime_est_typ_b
 * (0..11). If the device supports VIRTIO_BLK_F_LIFETIME and returns
 * S_OK, verify the response fields are within these spec ranges.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_lifetime_fields(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_BLK_F_LIFETIME)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_lifetime *life = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_GET_LIFETIME;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(life, 0xFF, sizeof(*life));
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(life), sizeof(*life),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

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
