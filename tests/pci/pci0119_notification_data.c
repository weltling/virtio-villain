/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0119: VIRTIO_F_NOTIFICATION_DATA split ring notification.
 *
 * Spec 4.1.5.2 / 2.7.23: when VIRTIO_F_NOTIFICATION_DATA is negotiated
 * the driver writes a 32 bit value to the queue notify address encoding
 * the virtqueue number in the low 16 bits and the available ring index
 * in the high 16 bits, instead of the bare virtqueue number. Submit a
 * block read, notify with the encoded 32 bit value, and confirm the
 * device still consumes the request. Skips when the device does not
 * offer the feature.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_notification_data(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_NOTIFICATION_DATA))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Notify with the encoded 32 bit value: vqn in low 16, available
     * ring index (1) in high 16, rather than the bare vqn. */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->queue_select = vr->queue;
    __sync_synchronize();
    uint16_t off = cfg->queue_notify_off;
    volatile uint32_t *naddr = (volatile uint32_t *)
        ((char *)dev->notify_base + off * dev->notify_off_multiplier);
    *naddr = (uint32_t)vr->queue | ((uint32_t)1 << 16);
    __sync_synchronize();

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != 0)
            return TEST_PASS;
        elapsed += 10000;
    }

    if (cfg->device_status == 0)
        return TEST_WEDGED;
    return TEST_REJECT;
}

REGISTER_TEST_REQUIRES(PCI0119, VIRTIO_PCI_DEVICE_BLK,
                       test_pci_notification_data,
                       "NOTIFICATION_DATA encoded split ring notify",
                       VIRTIO_SPEC_V1_2, "4.1.5.2",
                       VV_FEATURE_BIT(VIRTIO_F_NOTIFICATION_DATA), 0);
