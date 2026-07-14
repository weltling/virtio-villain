/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0035: free_page_hint
 *
 * Spec 5.5.6.3: When VIRTIO_BALLOON_F_FREE_PAGE_HINT is negotiated,
 * queue 2 is the free page hint VQ. The driver submits page range
 * descriptors as hints to the device about memory that is free but
 * not yet returned. The device consumes them via the used ring.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_free_page_hint(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1U << VIRTIO_BALLOON_F_FREE_PAGE_HINT)))
        return TEST_SKIP;

    /* Queue 2 is the free page hint VQ */
    cfg->queue_select = 2;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring fpq;
    vring_alloc(&fpq, 16);
    vring_attach(dev, &fpq, 2);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Submit a command ID descriptor followed by a page range.
     * The first buffer is 4 bytes containing the command ID. */
    uint32_t *cmd_id = vv_alloc_pages(1);
    *cmd_id = 2; /* arbitrary non-zero, non-STOP, non-DONE value */

    void *page = vv_alloc_pages(1);
    uint64_t cmd_phys = vv_virt_to_phys(cmd_id);
    uint64_t page_phys = vv_virt_to_phys(page);

    /* Desc 0: command ID (4 bytes, readable) */
    vring_raw_set_desc(&fpq, 0, cmd_phys, 4, VRING_DESC_F_NEXT, 1);
    /* Desc 1: free page range (one page, readable) */
    vring_raw_set_desc(&fpq, 1, page_phys, 4096, 0, 0);

    vring_raw_set_avail(&fpq, 0, 0);
    vring_raw_set_avail_idx(&fpq, 1);

    return vv_kick_and_wait(dev, &fpq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0035, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_free_page_hint,
              "Free page hint on hint virtqueue",
              VIRTIO_SPEC_V1_2, "5.5.6.3",
              (1ULL << VIRTIO_BALLOON_F_FREE_PAGE_HINT), 0);
