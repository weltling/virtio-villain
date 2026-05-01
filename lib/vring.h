/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_VRING_H
#define VV_VRING_H

#include <stdint.h>
#include "virtio_pci.h"

/* Vring descriptor flags */
#define VRING_DESC_F_NEXT     1
#define VRING_DESC_F_WRITE    2
#define VRING_DESC_F_INDIRECT 4

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

struct vring {
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    uint64_t desc_phys;
    uint64_t avail_phys;
    uint64_t used_phys;
    uint16_t size;
    uint16_t free_head;
    uint16_t queue;
};

/* Allocate vring structures (page aligned, locked). */
int vring_alloc(struct vring *vr, uint16_t size);

/* Attach vring to a virtio device queue. Sets addresses and enables. */
void vring_attach(struct virtio_dev *dev, struct vring *vr, uint16_t queue);

/* Submit a descriptor chain head to the available ring and advance idx. */
void vring_submit(struct vring *vr, uint16_t head);

/* Poll the used ring for a completion. Returns 0 on success, -1 on timeout. */
int vring_poll_used(struct vring *vr, uint32_t *id, uint32_t *len,
                    int timeout_ms);

/*
 * Raw manipulation - bypass safety for crafting malformed state.
 */
void vring_raw_set_desc(struct vring *vr, uint16_t slot,
                        uint64_t addr, uint32_t len,
                        uint16_t flags, uint16_t next);
void vring_raw_set_avail(struct vring *vr, uint16_t slot, uint16_t val);
void vring_raw_set_avail_idx(struct vring *vr, uint16_t idx);

#endif /* VV_VRING_H */
