/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0060: READ request spanning multiple queues with different sizes (spec 5.2.6)
 *
 * If multiqueue is supported, submit reads on two queues that were
 * configured with different queue_size values (if the device allows).
 * Tests device handling of heterogeneous queue configurations.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_multiqueue_sizes(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    if (nq < 2)
        return TEST_SKIP;

    /* Queue 0 is already set up with default size via vr.
     * Set up queue 1 with minimum size (could be different). */
    struct vring vr2;
    vring_alloc(&vr2, 16); /* smallest practical */
    vring_attach(dev, &vr2, 1);

    /* Submit on queue 0 */
    struct virtio_blk_outhdr *hdr0 = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *st0 = vv_alloc_pages(1);

    *st0 = 0xFF;

    vv_desc(vr, 0, hdr0, sizeof(*hdr0), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data0, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st0, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Submit on queue 1 */
    struct virtio_blk_outhdr *hdr1 = blk_hdr(VIRTIO_BLK_T_IN, 1);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *st1 = vv_alloc_pages(1);

    *st1 = 0xFF;

    vv_desc(&vr2, 0, hdr1, sizeof(*hdr1), VRING_DESC_F_NEXT, 1);
    vv_desc(&vr2, 1, data1, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(&vr2, 2, st1, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    /* Kick both */
    __sync_synchronize();
    uint16_t before0 = vr->used->idx;
    uint16_t before1 = vr2.used->idx;
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 1);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != before0 && vr2.used->idx != before1)
            return TEST_PASS;
        elapsed += 10000;
    }

    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(B0060, VIRTIO_PCI_DEVICE_BLK, test_blk_multiqueue_sizes,
              "Concurrent reads on two queues with different sizes",
              VIRTIO_SPEC_V1_2, "5.2.6");
