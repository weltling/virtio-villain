/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0024: Page report with zero length descriptor.
 *
 * Spec 5.5.6.4: Submit a report descriptor with len=0 on the
 * reporting virtqueue. The device must handle the degenerate
 * case without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BALLOON_F_PAGE_REPORTING 5

static test_result_t test_balloon_report_zero_len(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1U << VIRTIO_BALLOON_F_PAGE_REPORTING)))
        return TEST_SKIP;

    cfg->queue_select = 3;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring rq;
    vring_alloc(&rq, 16);
    vring_attach(dev, &rq, 3);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    void *page = vv_alloc_pages(1);
    uint64_t phys = vv_virt_to_phys(page);

    /* Zero length report */
    vring_raw_set_desc(&rq, 0, phys, 0, 0, 0);
    vring_raw_set_avail(&rq, 0, 0);
    vring_raw_set_avail_idx(&rq, 1);

    return vv_kick_and_wait(dev, &rq, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0024, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_report_zero_len,
              "Page report with zero length descriptor",
              VIRTIO_SPEC_V1_2, "5.5.6.4");
