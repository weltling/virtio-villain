/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0198: two reads in one batch both return S_OK.
 *
 * Submit two READ requests for sectors 0 and 1 in one avail batch.
 * Verify both complete with S_OK status.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_two_reads(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *h1 = vv_alloc_pages(1);
    uint8_t *d1 = vv_alloc_pages(1);
    uint8_t *s1 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *h2 = vv_alloc_pages(1);
    uint8_t *d2 = vv_alloc_pages(1);
    uint8_t *s2 = vv_alloc_pages(1);

    h1->type = VIRTIO_BLK_T_IN; h1->ioprio = 0; h1->sector = 0;
    h2->type = VIRTIO_BLK_T_IN; h2->ioprio = 0; h2->sector = 1;
    *s1 = 0xFF; *s2 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h1), sizeof(*h1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(d1), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(s1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(h2), sizeof(*h2),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(d2), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vring_raw_set_desc(vr, 5, vv_virt_to_phys(s2), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*s1 != VIRTIO_BLK_S_OK) TFAIL("read0 status %u", *s1);
    if (*s2 != VIRTIO_BLK_S_OK) TFAIL("read1 status %u", *s2);

    return TEST_PASS;
}

REGISTER_TEST(B0198, VIRTIO_PCI_DEVICE_BLK, test_blk_two_reads,
              "Two reads in one batch both return S_OK",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
