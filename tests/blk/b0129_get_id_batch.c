/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0129: Multiple GET_ID requests in a single batch.
 *
 * Submit two GET_ID requests as separate descriptor chains in one
 * avail ring update, testing batch processing.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_batch(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr0 = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *status0 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *hdr1 = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);

    *status0 = 0xFF;

    *status1 = 0xFF;

    /* First chain: descs 0-2 */
    vv_desc(vr, 0, hdr0, sizeof(*hdr0), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data0, VIRTIO_BLK_ID_BYTES,
            VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status0, 1, VRING_DESC_F_WRITE, 0);

    /* Second chain: descs 3-5 */
    vv_desc(vr, 3, hdr1, sizeof(*hdr1), VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, data1, VIRTIO_BLK_ID_BYTES,
            VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 5);
    vv_desc(vr, 5, status1, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx >= 2)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(B0129, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_batch,
              "Multiple GET_ID requests in one batch",
              VIRTIO_SPEC_V1_2, "5.2.6");
