/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0153: write on queue 0 then read same sector on queue 1
 *
 * Spec 5.2.6 says all queues operate on the same backing storage.
 * A write completed on one queue must be visible to a read on
 * another queue. Write a known pattern to sector 100 on queue 0,
 * wait for completion, then read sector 100 on queue 1 and verify
 * the data matches the pattern. Catches devices that fan out per
 * queue caches without coherence.
 */
#include "lib/virtio_test.h"

static int wait_used(struct vring *vr)
{
    int waited = 0;
    while (waited < VV_TIMEOUT_MS) {
        __sync_synchronize();
        if (vr->used->idx != 0)
            return 0;
        usleep(1000);
        waited++;
    }
    return -1;
}

static test_result_t test_blk_write_then_read_xq(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    (void)vr;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    cfg->device_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    cfg->driver_feature = cfg->device_feature;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = virtio_pci_base_features_hi(dev);
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring q0, q1;
    vring_alloc(&q0, 16);
    vring_alloc(&q1, 16);
    vring_attach(dev, &q0, 0);
    vring_attach(dev, &q1, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Write known pattern on queue 0 */
    struct virtio_blk_outhdr *whdr = blk_hdr(VIRTIO_BLK_T_OUT, 100);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);
    memset(wdata, 0xC3, 512);
    *wst = 0xFF;
    vv_desc(&q0, 0, whdr, sizeof(*whdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&q0, 1, wdata, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(&q0, 2, wst, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);
    virtio_pci_kick(dev, 0);
    if (wait_used(&q0) < 0)
        TWEDGED("wait_used(&q0) < 0");
    if (*wst != 0)
        TREJECT("*wst != 0");

    /* Read on queue 1 */
    struct virtio_blk_outhdr *rhdr = blk_hdr(VIRTIO_BLK_T_IN, 100);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);
    *rst = 0xFF;
    vv_desc(&q1, 0, rhdr, sizeof(*rhdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&q1, 1, rdata, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(&q1, 2, rst, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q1, 0, 0);
    vring_raw_set_avail_idx(&q1, 1);
    virtio_pci_kick(dev, 1);
    if (wait_used(&q1) < 0)
        TWEDGED("wait_used(&q1) < 0");
    if (*rst != 0)
        TFAIL("*rst != 0");

    for (int i = 0; i < 512; i++)
        if (rdata[i] != 0xC3)
            TFAIL("rdata[i] != 0xC3");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0153, VIRTIO_PCI_DEVICE_BLK, test_blk_write_then_read_xq,
              "write on q0 visible to read on q1 same sector",
              VIRTIO_SPEC_V1_2, "5.2.6",
              0, 2);
