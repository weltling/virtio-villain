/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0098: VIRTIO_F_NOTIFICATION_DATA encoded notify reaches device
 *
 * Spec 4.1.5.2 says when VIRTIO_F_NOTIFICATION_DATA (bit 38) is
 * negotiated, the driver must write a 32 bit value to the notify
 * register encoding the virtqueue number in the low 16 bits and
 * the next available index in the high 16 bits, instead of just
 * the queue number. Renegotiate features with the bit set, submit
 * a request, write the encoded notify value directly, and verify
 * the request completes. If the device does not advertise the
 * feature, skip.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_F_NOTIFICATION_DATA_BIT 38

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_notification_data(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Probe support */
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t hi = cfg->device_feature;
    if (!(hi & (1u << (VIRTIO_F_NOTIFICATION_DATA_BIT - 32))))
        return TEST_SKIP;

    /* Renegotiate with NOTIFICATION_DATA set */
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

    /* Build a normal block read */
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

    /* Encoded notify: vqn in low 16, next avail idx in high 16 */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t noff = cfg->queue_notify_off;
    volatile uint32_t *addr = (volatile uint32_t *)
        ((char *)dev->notify_base + (uint32_t)noff *
         dev->notify_off_multiplier);
    uint32_t encoded = (uint32_t)0 | ((uint32_t)1 << 16);
    *addr = encoded;
    __sync_synchronize();

    /* Wait for completion via used ring */
    for (int i = 0; i < VV_TIMEOUT_MS; i++) {
        __sync_synchronize();
        if (nv.used->idx != 0) {
            if (*st != 0)
                TFAIL("*st != 0");
            return TEST_PASS;
        }
        usleep(1000);
    }
    TWEDGED("device unresponsive after timeout");
}

REGISTER_TEST(T0098, VIRTIO_PCI_DEVICE_BLK, test_notification_data,
              "encoded notify with NOTIFICATION_DATA reaches device",
              VIRTIO_SPEC_V1_2, "4.1.5.2");
