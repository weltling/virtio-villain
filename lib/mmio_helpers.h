/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_MMIO_HELPERS_H
#define VV_MMIO_HELPERS_H

#include "virtio_test.h"

static inline test_result_t mmio_bind_queue(struct virtio_mmio_dev *dev,
                                            struct vring *ring,
                                            uint16_t queue)
{
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, queue);
    if (mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX) < ring->size)
        TFAIL("MMIO queue is too small for the ring");
    ring->queue = queue;
    ring->avail->flags = VRING_AVAIL_F_NO_INTERRUPT;
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, ring->size);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, ring->desc_phys);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, ring->desc_phys >> 32);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, ring->avail_phys);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, ring->avail_phys >> 32);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, ring->used_phys);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, ring->used_phys >> 32);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    return TEST_PASS;
}

static inline test_result_t mmio_attach_queue(struct virtio_mmio_dev *dev,
                                              struct vring *ring,
                                              uint16_t queue)
{
    if (vring_alloc(ring, 8) < 0)
        TFAIL("MMIO queue allocation failed");
    return mmio_bind_queue(dev, ring, queue);
}

static inline test_result_t mmio_watch_status(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        TFAIL("MMIO feature negotiation failed");
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 mmio_read32(dev, VIRTIO_MMIO_STATUS)
                 | VIRTIO_MMIO_STATUS_DRIVER_OK);
    puts("vv-mmio-ready");
    fflush(stdout);
    for (int elapsed = 0; elapsed <= 8000; elapsed += 100) {
        uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
        if (status & (VIRTIO_MMIO_STATUS_FAILED | VIRTIO_MMIO_STATUS_NEEDS_RESET))
            TFAIL("MMIO device raised failure status 0x%x", status);
        if (!(status & VIRTIO_MMIO_STATUS_DRIVER_OK))
            TFAIL("MMIO device lost DRIVER_OK");
        if (elapsed < 8000)
            usleep(100000);
    }
    return TEST_PASS;
}

#endif
