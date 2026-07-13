/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0026: Page report concurrent with inflate.
 *
 * Spec 5.5.6.4: Submit a page report on queue 3 and an inflate
 * request on queue 0 simultaneously. Both queues are independent
 * and the device must service them without deadlock.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>


static test_result_t test_balloon_report_with_inflate(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1U << VIRTIO_BALLOON_F_REPORTING)))
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

    /* Inflate on queue 0 */
    uint32_t *pfns = vv_alloc_pages(1);
    void *pages = vv_alloc_pages(4);
    uint64_t base = vv_virt_to_phys(pages);
    for (int i = 0; i < 4; i++)
        pfns[i] = (uint32_t)((base >> VIRTIO_BALLOON_PFN_SHIFT) + i);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pfns),
                       4 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Report on queue 3 */
    void *rpage = vv_alloc_pages(1);
    vring_raw_set_desc(&rq, 0, vv_virt_to_phys(rpage), 4096, 0, 0);
    vring_raw_set_avail(&rq, 0, 0);
    vring_raw_set_avail_idx(&rq, 1);

    /* Kick both queues */
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 3);

    /* Wait for either to complete */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx >= 1 || rq.used->idx >= 1)
            return TEST_PASS;
        elapsed += 10000;
    }

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_REQUIRES(L0026, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_report_with_inflate,
              "Page report concurrent with inflate",
              VIRTIO_SPEC_V1_2, "5.5.6.4",
              (1ULL << VIRTIO_BALLOON_F_REPORTING), 0);
