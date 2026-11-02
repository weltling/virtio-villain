/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0039: deflate one page returns buffer via used ring.
 *
 * Spec 5.5.6.1: The deflate queue (queue 1) accepts 4 byte PFN
 * entries indicating pages to return to the guest. Submit one
 * valid PFN and verify the device consumes it.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_deflate_valid(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Deflate queue is queue 1 */
    cfg->queue_select = 1;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring dq;
    vring_alloc(&dq, 16);
    vring_attach(dev, &dq, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Submit one PFN to deflate */
    uint32_t *pfn_buf = vv_alloc_pages(1);
    void *page = vv_alloc_pages(1);
    pfn_buf[0] = (uint32_t)(vv_virt_to_phys(page) >> VIRTIO_BALLOON_PFN_SHIFT);

    vv_desc(&dq, 0, pfn_buf, 4, 0, 0);
    vring_raw_set_avail(&dq, 0, 0);
    vring_raw_set_avail_idx(&dq, 1);

    return vv_kick_and_wait(dev, &dq, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV(L0039, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_deflate_valid,
                  "Deflate one page on deflate queue",
                  VIRTIO_SPEC_V1_2, "5.5.6.1");
