/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0046: reporting_misaligned_len
 *
 * Spec 5.5.6.4: elements on the free page reporting virtqueue describe
 * ranges of free guest pages. A reported range is a whole number of
 * pages, so its length is a multiple of the page size. Here the driver
 * submits a reporting element whose length is not page aligned. A
 * device that rounds the length up or frees a partial page could
 * corrupt a page the guest still owns. It must reject the element or
 * ignore the misaligned tail, and stay healthy.
 */
#include "lib/virtio_test.h"

static test_result_t test_balloon_reporting_misaligned_len(
    struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (!virtio_pci_feature_offered(dev, VIRTIO_BALLOON_F_REPORTING))
        return TEST_SKIP;

    /* Queue 3 is the reporting VQ. */
    cfg->queue_select = 3;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring rq;
    vring_alloc(&rq, 16);
    vring_attach(dev, &rq, 3);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Two pages backing the range, but a length that is not a page
     * multiple so the reported range ends inside the second page. */
    void *page = vv_alloc_pages(2);

    vv_desc(&rq, 0, page, 4096 + 100, 0, 0);
    vring_raw_set_avail(&rq, 0, 0);
    vring_raw_set_avail_idx(&rq, 1);

    return vv_kick_and_wait(dev, &rq, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV_REQUIRES(L0046, VIRTIO_PCI_DEVICE_BALLOON,
                  test_balloon_reporting_misaligned_len,
                  "Reporting element with a non page aligned length",
                  VIRTIO_SPEC_V1_2, "5.5.6.4",
                  (1ULL << VIRTIO_BALLOON_F_REPORTING), 0);
