/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0115: two request chains processed from one kick.
 *
 * Spec 2.7.13: The device processes all available descriptors after
 * a notification. Post two independent request chains in the avail
 * ring and kick once. Both must complete.
 */
#include "lib/virtio_test.h"

static test_result_t test_two_chains(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *h1 = vv_alloc_pages(1);
    uint8_t *d1 = vv_alloc_pages(1);
    uint8_t *s1 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *h2 = vv_alloc_pages(1);
    uint8_t *d2 = vv_alloc_pages(1);
    uint8_t *s2 = vv_alloc_pages(1);

    h1->type = VIRTIO_BLK_T_IN; h1->ioprio = 0; h1->sector = 0;
    h2->type = VIRTIO_BLK_T_IN; h2->ioprio = 0; h2->sector = 0;
    *s1 = 0xFF; *s2 = 0xFF;

    /* Chain 1: descs 0,1,2 */
    vv_desc(vr, 0, h1, sizeof(*h1), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, d1, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, s1, 1, VRING_DESC_F_WRITE, 0);

    /* Chain 2: descs 3,4,5 */
    vv_desc(vr, 3, h2, sizeof(*h2), VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, d2, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vv_desc(vr, 5, s2, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (*s1 != VIRTIO_BLK_S_OK) TFAIL("chain1 status %u", *s1);
    if (*s2 != VIRTIO_BLK_S_OK) TFAIL("chain2 status %u", *s2);

    return TEST_PASS;
}

REGISTER_TEST(T0115, VIRTIO_PCI_DEVICE_BLK, test_two_chains,
              "Two chains processed from one kick",
              VIRTIO_SPEC_V1_2, "2.7.13");
