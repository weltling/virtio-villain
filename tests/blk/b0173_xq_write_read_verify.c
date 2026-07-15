/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0173: cross queue write then read verifies data.
 *
 * With VIRTIO_BLK_F_MQ negotiated (at least 2 request queues),
 * write a known pattern on queue 0 then read the same sector on
 * queue 1. Verify the data matches. This tests that different
 * queues share coherent access to the backing store.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_xq_write_read_verify(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* Reinit with both queues */
    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0 = cfg->device_feature;
    cfg->driver_feature_select = 0;
    cfg->driver_feature = f0;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t f1 = cfg->device_feature;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = f1;
    __sync_synchronize();

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

    /* Write on queue 0 */
    struct virtio_blk_outhdr *whdr = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);

    whdr->type = VIRTIO_BLK_T_OUT;
    whdr->ioprio = 0;
    whdr->sector = 0;
    for (int i = 0; i < 512; i++)
        wdata[i] = (uint8_t)(i ^ 0x5A);
    *wst = 0xFF;

    vring_raw_set_desc(&q0, 0, vv_virt_to_phys(whdr), sizeof(*whdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q0, 1, vv_virt_to_phys(wdata), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&q0, 2, vv_virt_to_phys(wst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);

    test_result_t r = vv_kick_and_wait(dev, &q0, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (*wst != VIRTIO_BLK_S_OK)
        TFAIL("write status %u", *wst);

    /* Read on queue 1 */
    struct virtio_blk_outhdr *rhdr = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    rhdr->type = VIRTIO_BLK_T_IN;
    rhdr->ioprio = 0;
    rhdr->sector = 0;
    memset(rdata, 0, 512);
    *rst = 0xFF;

    vring_raw_set_desc(&q1, 0, vv_virt_to_phys(rhdr), sizeof(*rhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q1, 1, vv_virt_to_phys(rdata), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&q1, 2, vv_virt_to_phys(rst), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q1, 0, 0);
    vring_raw_set_avail_idx(&q1, 1);

    r = vv_kick_and_wait(dev, &q1, 1, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (*rst != VIRTIO_BLK_S_OK)
        TFAIL("read status %u", *rst);

    /* Verify data integrity across queues */
    for (int i = 0; i < 512; i++) {
        uint8_t expected = (uint8_t)(i ^ 0x5A);
        if (rdata[i] != expected)
            TFAIL("byte %d: got 0x%02x, want 0x%02x",
                  i, rdata[i], expected);
    }
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0173, VIRTIO_PCI_DEVICE_BLK,
              test_blk_xq_write_read_verify,
              "Cross queue write then read verifies data coherence",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_MQ), 2);
