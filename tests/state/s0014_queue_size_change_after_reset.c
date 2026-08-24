/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0014: queue_size_change_after_reset
 *
 * Initialize queue 0 with one size, perform I/O, reset the device,
 * then re-initialize queue 0 with a DIFFERENT queue_size. The spec
 * allows this (driver can choose any power-of-2 <= device max), but
 * a VMM that caches the original queue_size and doesn't re-read it
 * on reset will use the wrong ring dimensions, potentially reading
 * out-of-bounds on the descriptor/avail/used arrays.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_queue_size_change(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Phase 1: Do I/O on current queue (size=16, set by harness).
     */
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

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return TEST_SKIP;

    /*
     * Phase 2: Reset and re-init with a SMALLER queue size.
     */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    /* Check max queue size */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t max_size = cfg->queue_size;
    if (max_size < 8) {
        /* Device max is too small to test with different sizes */
        return TEST_SKIP;
    }

    /* Use size=8 (different from the original 16) */
    uint16_t new_size = 8;
    struct vring vr2;
    vring_alloc(&vr2, new_size);

    cfg->queue_size = new_size;
    virtio_store64(&cfg->queue_desc, vr2.desc_phys);
    virtio_store64(&cfg->queue_avail, vr2.avail_phys);
    virtio_store64(&cfg->queue_used, vr2.used_phys);
    cfg->queue_msix_vector = 0xffff;
    cfg->queue_enable = 1;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /*
     * Phase 3: Do I/O on the resized queue.
     * If VMM still uses old size=16, it may read past our size=8 arrays.
     */
    struct virtio_blk_outhdr *hdr2 = vv_alloc_pages(1);
    uint8_t *data2 = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    hdr2->type = VIRTIO_BLK_T_IN;
    hdr2->ioprio = 0;
    hdr2->sector = 0;
    *status2 = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr2), sizeof(*hdr2),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data2), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status2), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0014, VIRTIO_PCI_DEVICE_BLK, test_queue_size_change,
              "Reset and re-init queue with different size",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
