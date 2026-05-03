/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_VIRTIO_MMIO_H
#define VV_VIRTIO_MMIO_H

#include <stdint.h>

/* MMIO register offsets (spec 4.2.2) */
#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW    0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH   0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW    0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH   0x0a4
#define VIRTIO_MMIO_SHM_SEL            0x0ac
#define VIRTIO_MMIO_SHM_LEN_LOW        0x0b0
#define VIRTIO_MMIO_SHM_LEN_HIGH       0x0b4
#define VIRTIO_MMIO_SHM_BASE_LOW       0x0b8
#define VIRTIO_MMIO_SHM_BASE_HIGH      0x0bc
#define VIRTIO_MMIO_QUEUE_RESET        0x0c0
#define VIRTIO_MMIO_CONFIG_GENERATION   0x0fc
#define VIRTIO_MMIO_CONFIG              0x100

/* Expected magic value "virt" in little-endian */
#define VIRTIO_MMIO_MAGIC  0x74726976

/* MMIO region size per device */
#define VIRTIO_MMIO_REGION_SIZE  0x200

/* Virtio device status bits (same as PCI, spec 2.1) */
#define VIRTIO_MMIO_STATUS_ACKNOWLEDGE  1
#define VIRTIO_MMIO_STATUS_DRIVER       2
#define VIRTIO_MMIO_STATUS_DRIVER_OK    4
#define VIRTIO_MMIO_STATUS_FEATURES_OK  8
#define VIRTIO_MMIO_STATUS_NEEDS_RESET  64
#define VIRTIO_MMIO_STATUS_FAILED       128

/*
 * Parsed MMIO virtio device.
 */
struct virtio_mmio_dev {
    volatile void *base;      /* MMIO register base */
    uint64_t phys_base;       /* Physical base address */
    uint32_t size;            /* Region size */
    uint32_t device_id;       /* Device type from DeviceID register */
};

/* Read/write helpers for MMIO registers (always 32-bit as per spec). */
static inline uint32_t mmio_read32(struct virtio_mmio_dev *dev, uint32_t offset)
{
    return *(volatile uint32_t *)((char *)dev->base + offset);
}

static inline void mmio_write32(struct virtio_mmio_dev *dev, uint32_t offset,
                                uint32_t val)
{
    *(volatile uint32_t *)((char *)dev->base + offset) = val;
    __sync_synchronize();
}

/* Wrong-width access helpers for tests (intentional violations). */
static inline uint8_t mmio_read8(struct virtio_mmio_dev *dev, uint32_t offset)
{
    return *(volatile uint8_t *)((char *)dev->base + offset);
}

static inline uint16_t mmio_read16(struct virtio_mmio_dev *dev, uint32_t offset)
{
    return *(volatile uint16_t *)((char *)dev->base + offset);
}

static inline void mmio_write8(struct virtio_mmio_dev *dev, uint32_t offset,
                               uint8_t val)
{
    *(volatile uint8_t *)((char *)dev->base + offset) = val;
    __sync_synchronize();
}

static inline void mmio_write16(struct virtio_mmio_dev *dev, uint32_t offset,
                                uint16_t val)
{
    *(volatile uint16_t *)((char *)dev->base + offset) = val;
    __sync_synchronize();
}

/*
 * Find an MMIO virtio device by scanning device tree or platform devices.
 * Returns 0 on success.
 */
int virtio_mmio_find(struct virtio_mmio_dev *dev);

/*
 * Perform MMIO virtio device initialization (reset → DRIVER_OK).
 * Returns 0 on success.
 */
int virtio_mmio_init(struct virtio_mmio_dev *dev);

/* Reset MMIO device (write 0 to Status register). */
void virtio_mmio_reset(struct virtio_mmio_dev *dev);

/* Kick a queue via QueueNotify register. */
void virtio_mmio_kick(struct virtio_mmio_dev *dev, uint16_t queue);

#endif /* VV_VIRTIO_MMIO_H */
