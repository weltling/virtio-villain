/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0102: NOTIFICATION_DATA with stale avail_idx.
 *
 * Spec 4.1.5.2: The high 16 bits encode the next available index.
 * Write an encoded value where avail_idx is 0 (stale) while the
 * actual avail ring has advanced to 1. The device may use this
 * as a hint and must not crash on the discrepancy.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_notif_data_stale_idx(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t hi = cfg->device_feature;
    if (!(hi & (1u << (VIRTIO_F_NOTIFICATION_DATA - 32))))
        return TEST_SKIP;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0 = cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t f1 = cfg->device_feature;

    cfg->driver_feature_select = 0;
    cfg->driver_feature = f0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = f1;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    struct vring nv;
    vring_alloc(&nv, 16);
    vring_attach(dev, &nv, 0);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(&nv, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&nv, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&nv, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&nv, 0, 0);
    vring_raw_set_avail_idx(&nv, 1);

    /* Notify with stale avail_idx = 0 (actual is 1) */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t noff = cfg->queue_notify_off;
    volatile uint32_t *addr = (volatile uint32_t *)
        ((char *)dev->notify_base + (uint32_t)noff *
         dev->notify_off_multiplier);
    uint32_t encoded = (uint32_t)0 | ((uint32_t)0 << 16); /* stale */
    *addr = encoded;
    __sync_synchronize();

    /* Give time, then do correct notify */
    usleep(100000);
    encoded = (uint32_t)0 | ((uint32_t)1 << 16);
    *addr = encoded;
    __sync_synchronize();

    for (int i = 0; i < VV_TIMEOUT_MS; i++) {
        __sync_synchronize();
        if (nv.used->idx != 0)
            return TEST_PASS;
        usleep(1000);
    }

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0102, VIRTIO_PCI_DEVICE_BLK, test_notif_data_stale_idx,
              "NOTIFICATION_DATA with stale avail index",
              VIRTIO_SPEC_V1_2, "4.1.5.2");
