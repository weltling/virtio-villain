/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0109: NOTIFICATION_DATA correct encoding processes request.
 *
 * Spec 4.1.5.2: When VIRTIO_F_NOTIFICATION_DATA is negotiated, the
 * notification is a 32-bit write encoding vq_index in the low 16 bits
 * and next_off (avail index, 15 bits) plus next_wrap (bit 15) in the
 * high 16 bits. Encode this correctly and verify the device processes
 * the request.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_notif_data_correct(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t hi = cfg->device_feature;
    if (!(hi & (1u << (VIRTIO_F_NOTIFICATION_DATA - 32))))
        return TEST_SKIP;

    /* Reinit with NOTIFICATION_DATA negotiated */
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
        TREJECT("FEATURES_OK not set");

    struct vring nv;
    vring_alloc(&nv, 16);
    vring_attach(dev, &nv, 0);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Build a simple read request */
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
    __sync_synchronize();

    /* Send notification with correct encoding:
     * bits [15:0]  = vq_index (0)
     * bits [30:16] = next_off (avail_idx & 0x7FFF = 1)
     * bit  [31]    = next_wrap (avail_idx bit 15 = 0)
     */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t noff = cfg->queue_notify_off;
    volatile uint32_t *addr = (volatile uint32_t *)
        ((char *)dev->notify_base + (uint32_t)noff *
         dev->notify_off_multiplier);
    uint32_t vq_idx = 0;
    uint32_t next_off = 1;  /* next avail index */
    uint32_t next_wrap = 0;
    uint32_t encoded = vq_idx | (next_off << 16) | (next_wrap << 31);
    *addr = encoded;
    __sync_synchronize();

    /* Wait for completion */
    uint16_t before = nv.used->idx;
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if ((uint16_t)(nv.used->idx - before) >= 1)
            return TEST_PASS;
        elapsed += 10000;
    }

    return TEST_REJECT;
}

REGISTER_TEST_REQUIRES(T0109, VIRTIO_PCI_DEVICE_BLK, test_notif_data_correct,
              "NOTIFICATION_DATA with correct encoding processes request",
              VIRTIO_SPEC_V1_4, "4.1.5.2",
              (1ULL << VIRTIO_F_NOTIFICATION_DATA), 0);
