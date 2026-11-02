/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_BLK_MMIO_HELPERS_H
#define VV_BLK_MMIO_HELPERS_H

#include "lib/virtio_test.h"
#include "tests/mmio/mmio_helpers.h"

static inline test_result_t blk_mmio_attach(struct virtio_mmio_dev *dev,
                                           struct vring *ring)
{
    if (dev->device_id != 2)
        TSKIP("MMIO device is not virtio-blk");
    if (virtio_mmio_init(dev) < 0)
        TFAIL("MMIO feature negotiation failed");
    VV_TRY(mmio_attach_queue(dev, ring, 0));
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 mmio_read32(dev, VIRTIO_MMIO_STATUS)
                 | VIRTIO_MMIO_STATUS_DRIVER_OK);
    return TEST_PASS;
}

static inline test_result_t blk_mmio_read(struct virtio_mmio_dev *dev,
                                         struct vring *ring, uint64_t sector)
{
    struct virtio_blk_outhdr *header = blk_hdr(VIRTIO_BLK_T_IN, sector);
    void *data = vv_alloc_pages(1);
    volatile uint8_t *status = vv_alloc_pages(1);
    *status = 0xff;
    vv_desc(ring, 0, header, sizeof(*header), VRING_DESC_F_NEXT, 1);
    vv_desc(ring, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(ring, 2, (void *)status, 1, VRING_DESC_F_WRITE, 0);
    uint16_t before = ring->used->idx;
    vring_submit(ring, 0);
    virtio_mmio_kick(dev, 0);
    for (int i = 0; i < VV_TIMEOUT_MS * 4 * vv_wait_scale; i++) {
        __sync_synchronize();
        if (ring->used->idx != before) {
            if (*status != 0)
                TFAIL("MMIO block read status is %u", *status);
            return TEST_PASS;
        }
        usleep(1000);
    }
    TWEDGED("MMIO block read did not complete");
}

static inline test_result_t blk_mmio_host_cycle(struct virtio_mmio_dev *dev)
{
    struct vring ring;
    test_result_t result = blk_mmio_attach(dev, &ring);
    if (result != TEST_PASS)
        return result;
    result = blk_mmio_read(dev, &ring, 0);
    if (result != TEST_PASS)
        return result;
    puts("vv-mmio-ready");
    fflush(stdout);
    sleep(2);
    return blk_mmio_read(dev, &ring, 1);
}

#endif
