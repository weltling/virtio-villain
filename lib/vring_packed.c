/* SPDX-License-Identifier: Apache-2.0 */
#include "vring_packed.h"
#include "util.h"

#include <string.h>
#include <unistd.h>

int vring_packed_alloc(struct vring_packed *vr, uint16_t size)
{
    vr->size = size;
    vr->next_avail = 0;
    vr->wrap_counter = 1;

    /* Descriptor ring: size * 16 bytes, page-aligned */
    size_t desc_pages = (size * sizeof(struct vring_packed_desc) +
                         PAGE_SIZE - 1) / PAGE_SIZE;
    vr->desc = vv_alloc_pages(desc_pages ? desc_pages : 1);
    vr->desc_phys = vv_virt_to_phys(vr->desc);

    /* Driver event suppression (4 bytes, page-aligned) */
    vr->driver_event = vv_alloc_pages(1);
    vr->driver_event_phys = vv_virt_to_phys(vr->driver_event);

    /* Device event suppression (4 bytes, page-aligned) */
    vr->device_event = vv_alloc_pages(1);
    vr->device_event_phys = vv_virt_to_phys(vr->device_event);

    return 0;
}

void vring_packed_attach(struct virtio_dev *dev, struct vring_packed *vr,
                         uint16_t queue)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    vr->queue = queue;
    cfg->queue_select = queue;
    __sync_synchronize();
    cfg->queue_size = vr->size;
    cfg->queue_desc = vr->desc_phys;
    cfg->queue_avail = vr->driver_event_phys;
    cfg->queue_used = vr->device_event_phys;
    cfg->queue_msix_vector = 0xffff;
    cfg->queue_enable = 1;
    __sync_synchronize();
}

int virtio_pci_init_packed(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset. Per spec 4.1.4.3.1 poll for completion. */
    cfg->device_status = 0;
    __sync_synchronize();
    for (int i = 0; i < 1000; i++) {
        __sync_synchronize();
        if (cfg->device_status == 0)
            break;
        usleep(1000);
    }
    if (cfg->device_status != 0)
        return -1;

    /* Acknowledge */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    /* Driver */
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Check if device offers VIRTIO_F_RING_PACKED (bit 34 = page 1, bit 2) */
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t features_hi = cfg->device_feature;
    if (!(features_hi & (1u << (VIRTIO_F_RING_PACKED - 32))))
        return -1;

    /* Negotiate only VIRTIO_F_RING_PACKED */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = (1u << (VIRTIO_F_RING_PACKED - 32));
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    /* Poll for FEATURES_OK acceptance (spec 2.2.2 / 2.1.2). */
    for (int i = 0; i < 1000; i++) {
        __sync_synchronize();
        if (cfg->device_status & VIRTIO_STATUS_FEATURES_OK)
            return 0;
        usleep(1000);
    }
    return -1;
}
