/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0066: ring_reset_reenable_different_size
 *
 * Reset a queue, then re-enable it with a different queue size.
 * Spec 2.2.1: the device MUST observe any queue configuration
 * that may have been changed by the driver, like the maximum
 * queue size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_F_RING_RESET 40
#define VIRTIO_BLK_T_IN 0

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

static test_result_t test_ring_reset_new_size(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check for RING_RESET */
    cfg->device_feature_select = VIRTIO_F_RING_RESET / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_RING_RESET % 32))))
        return TEST_SKIP;

    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t orig_size = cfg->queue_size;

    if (!cfg->queue_enable || orig_size < 32)
        return TEST_SKIP;

    /* Reset queue 0 */
    cfg->queue_enable = 0;
    __sync_synchronize();
    usleep(50000);

    /* Re-enable with smaller size (half of original, minimum 16) */
    uint16_t new_size = orig_size / 2;
    if (new_size < 16)
        new_size = 16;

    struct vring vr2;
    vring_alloc(&vr2, new_size);

    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_size = new_size;
    cfg->queue_desc = vr2.desc_phys;
    cfg->queue_avail = vr2.avail_phys;
    cfg->queue_used = vr2.used_phys;
    cfg->queue_enable = 1;
    __sync_synchronize();
    usleep(10000);

    /* Verify new size is applied */
    uint16_t readback = cfg->queue_size;
    if (readback != new_size)
        TFAIL("readback != new_size");

    /* Verify I/O works on re-enabled queue */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0066, VIRTIO_PCI_DEVICE_BLK, test_ring_reset_new_size,
              "Re-enable queue with different size after RING_RESET",
              VIRTIO_SPEC_V1_3, "2.2.1");
