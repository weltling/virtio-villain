/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_VRING_PACKED_H
#define VV_VRING_PACKED_H

#include <stdint.h>
#include "virtio_pci.h"

/* Packed descriptor flags (spec 2.8.4) */
#define VRING_PACKED_DESC_F_NEXT     1
#define VRING_PACKED_DESC_F_WRITE    2
#define VRING_PACKED_DESC_F_INDIRECT 4
#define VRING_PACKED_DESC_F_AVAIL    (1 << 7)
#define VRING_PACKED_DESC_F_USED     (1 << 15)

/* Event suppression flags, the low two bits of the event flags field
 * (spec 2.8.10). RESERVED is the invalid encoding. */
#define VRING_PACKED_EVENT_FLAG_ENABLE   0
#define VRING_PACKED_EVENT_FLAG_DISABLE  1
#define VRING_PACKED_EVENT_FLAG_DESC     2
#define VRING_PACKED_EVENT_FLAG_RESERVED 3

/* VIRTIO_F_RING_PACKED feature bit */
#define VIRTIO_F_RING_PACKED 34

struct vring_packed_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t id;
    uint16_t flags;
} __attribute__((packed));

/*
 * Driver event suppression area (spec 2.8.10).
 * Used for both driver and device event suppression.
 */
struct vring_packed_event {
    uint16_t off_wrap;
    uint16_t flags;
} __attribute__((packed));

struct vring_packed {
    struct vring_packed_desc  *desc;
    struct vring_packed_event *driver_event;
    struct vring_packed_event *device_event;
    uint64_t desc_phys;
    uint64_t driver_event_phys;
    uint64_t device_event_phys;
    uint16_t size;
    uint16_t next_avail;
    uint16_t queue;
    uint8_t  wrap_counter;
};

/* Allocate packed vring structures (page aligned). */
int vring_packed_alloc(struct vring_packed *vr, uint16_t size);

/*
 * Attach packed vring to a virtio device queue.
 * Sets addresses and enables the queue.
 */
void vring_packed_attach(struct virtio_dev *dev, struct vring_packed *vr,
                         uint16_t queue);

/*
 * Initialize a virtio device negotiating VIRTIO_F_RING_PACKED.
 * Returns 0 on success, -1 if device does not offer packed queues.
 */
int virtio_pci_init_packed(struct virtio_dev *dev);

/*
 * Set a packed descriptor with proper AVAIL/USED flag handling.
 * wrap_counter determines the phase of the AVAIL bit.
 */
static inline void vring_packed_set_desc(struct vring_packed *vr,
                                         uint16_t idx, uint64_t addr,
                                         uint32_t len, uint16_t id,
                                         uint16_t flags)
{
    uint16_t avail_bit = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    uint16_t used_bit = vr->wrap_counter ? 0 : VRING_PACKED_DESC_F_USED;

    vr->desc[idx].addr = addr;
    vr->desc[idx].len = len;
    vr->desc[idx].id = id;
    /* Set all flags atomically including AVAIL/USED */
    __sync_synchronize();
    vr->desc[idx].flags = flags | avail_bit | used_bit;
    __sync_synchronize();
}

/*
 * Raw set - no AVAIL/USED bit management. For crafting malformed state.
 */
static inline void vring_packed_raw_set_desc(struct vring_packed *vr,
                                             uint16_t idx, uint64_t addr,
                                             uint32_t len, uint16_t id,
                                             uint16_t flags)
{
    vr->desc[idx].addr = addr;
    vr->desc[idx].len = len;
    vr->desc[idx].id = id;
    vr->desc[idx].flags = flags;
}

/* Advance the next_avail index, wrapping and flipping wrap_counter. */
static inline void vring_packed_advance(struct vring_packed *vr)
{
    vr->next_avail++;
    if (vr->next_avail >= vr->size) {
        vr->next_avail = 0;
        vr->wrap_counter ^= 1;
    }
}

/* Check if a descriptor has been used by the device. */
static inline int vring_packed_desc_is_used(struct vring_packed *vr,
                                            uint16_t idx,
                                            uint8_t wrap_counter)
{
    __sync_synchronize();
    uint16_t flags = vr->desc[idx].flags;
    int avail = !!(flags & VRING_PACKED_DESC_F_AVAIL);
    int used = !!(flags & VRING_PACKED_DESC_F_USED);
    /* Descriptor is used when avail == used == wrap_counter */
    return (avail == wrap_counter) && (used == wrap_counter);
}

#endif /* VV_VRING_PACKED_H */
