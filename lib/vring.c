/* SPDX-License-Identifier: Apache-2.0 */
#include "vring.h"
#include "util.h"

#include <string.h>
#include <unistd.h>

int vring_alloc(struct vring *vr, uint16_t size)
{
    vr->size = size;
    vr->free_head = 0;

    vr->desc = vv_alloc_pages(1);
    vr->avail = vv_alloc_pages(1);
    vr->used = vv_alloc_pages(1);

    vr->desc_phys = vv_virt_to_phys(vr->desc);
    vr->avail_phys = vv_virt_to_phys(vr->avail);
    vr->used_phys = vv_virt_to_phys(vr->used);

    return 0;
}

void vring_attach(struct virtio_dev *dev, struct vring *vr, uint16_t queue)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    vr->queue = queue;
    cfg->queue_select = queue;
    __sync_synchronize();
    cfg->queue_size = vr->size;
    cfg->queue_desc = vr->desc_phys;
    cfg->queue_avail = vr->avail_phys;
    cfg->queue_used = vr->used_phys;
    cfg->queue_msix_vector = 0xffff;
    // The harness polls the used ring, so tell the device never to raise
    // a used buffer interrupt. Otherwise a device the kernel bound with
    // no matching driver storms the shared INTx line into "nobody cared".
    vr->avail->flags = VRING_AVAIL_F_NO_INTERRUPT;
    cfg->queue_enable = 1;
    __sync_synchronize();
}

void vring_submit(struct vring *vr, uint16_t head)
{
    uint16_t idx = vr->avail->idx;
    vr->avail->ring[idx % vr->size] = head;
    __sync_synchronize();
    vr->avail->idx = idx + 1;
    __sync_synchronize();
}

int vring_poll_used(struct vring *vr, uint32_t *id, uint32_t *len,
                    int timeout_ms)
{
    static uint16_t last_used_idx = 0;

    for (int i = 0; i < timeout_ms; i++) {
        __sync_synchronize();
        if (vr->used->idx != last_used_idx) {
            uint16_t slot = last_used_idx % vr->size;
            if (id) *id = vr->used->ring[slot].id;
            if (len) *len = vr->used->ring[slot].len;
            last_used_idx++;
            return 0;
        }
        usleep(1000);
    }
    return -1;
}

void vring_raw_set_desc(struct vring *vr, uint16_t slot,
                        uint64_t addr, uint32_t len,
                        uint16_t flags, uint16_t next)
{
    vr->desc[slot].addr = addr;
    vr->desc[slot].len = len;
    vr->desc[slot].flags = flags;
    vr->desc[slot].next = next;
}

void vring_raw_set_avail(struct vring *vr, uint16_t slot, uint16_t val)
{
    vr->avail->ring[slot] = val;
}

void vring_raw_set_avail_idx(struct vring *vr, uint16_t idx)
{
    __sync_synchronize();
    vr->avail->idx = idx;
    __sync_synchronize();
}
