/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0029: Page reporting descriptor with length not a 4096 multiple.
 *
 * Spec 5.5.6.4: Each reported range must be page aligned. Submit
 * a reporting descriptor with len = 4097 (one extra byte past a
 * page boundary). The device must reject or process only the
 * aligned prefix without dereferencing the trailing byte as a
 * new page.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>


static test_result_t test_balloon_page_report_misaligned(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BALLOON_F_REPORTING)))
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

    /* Two contiguous pages so a 4097 byte range stays inside our buffer */
    void *page = vv_alloc_pages(2);
    uint64_t phys = vv_virt_to_phys(page);

    vring_raw_set_desc(&rq, 0, phys, 4097, 0, 0);
    vring_raw_set_avail(&rq, 0, 0);
    vring_raw_set_avail_idx(&rq, 1);

    return vv_kick_and_wait(dev, &rq, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0029, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_page_report_misaligned,
              "Page report descriptor with len not a 4096 multiple",
              VIRTIO_SPEC_V1_2, "5.5.6.4",
              (1ULL << VIRTIO_BALLOON_F_REPORTING), 0);
