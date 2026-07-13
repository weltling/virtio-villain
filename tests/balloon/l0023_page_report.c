/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0023: Submit a valid page report on the reporting virtqueue.
 *
 * Spec 5.5.6.4: When VIRTIO_BALLOON_F_REPORTING is negotiated
 * queue 3 is the reporting VQ. The driver submits device readable
 * descriptors whose addr/len describe free page ranges. The device
 * consumes them and returns the buffers via the used ring.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>


static test_result_t test_balloon_page_report(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1U << VIRTIO_BALLOON_F_REPORTING)))
        return TEST_SKIP;

    /* Queue 3 is the reporting VQ */
    cfg->queue_select = 3;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring rq;
    vring_alloc(&rq, 16);
    vring_attach(dev, &rq, 3);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Submit a single page report descriptor */
    void *page = vv_alloc_pages(1);
    uint64_t phys = vv_virt_to_phys(page);

    vring_raw_set_desc(&rq, 0, phys, 4096, 0, 0);
    vring_raw_set_avail(&rq, 0, 0);
    vring_raw_set_avail_idx(&rq, 1);

    return vv_kick_and_wait(dev, &rq, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0023, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_page_report,
              "Page report on reporting virtqueue",
              VIRTIO_SPEC_V1_2, "5.5.6.4",
              (1ULL << VIRTIO_BALLOON_F_REPORTING), 0);
