/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0045: page reporting preserves poison value.
 *
 * Spec 5.5.6.4 (device-normative): when both
 * VIRTIO_BALLOON_F_PAGE_POISON and VIRTIO_BALLOON_F_REPORTING are
 * negotiated the device MUST NOT modify a reported page to any value
 * other than poison_val. Fill a page with a known pattern, report it
 * on the reporting virtqueue, and after the report completes verify
 * every word is either the original pattern (left untouched) or the
 * configured poison_val. Any other value violates the requirement.
 * Skips when the device does not offer both features.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define BALLOON_CFG_POISON_VAL_OFFSET 12
#define FILL_PATTERN 0xA5A5A5A5u

static test_result_t test_balloon_report_poison(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1U << VIRTIO_BALLOON_F_REPORTING)) ||
        !(feat & (1U << VIRTIO_BALLOON_F_PAGE_POISON)))
        return TEST_SKIP;

    if (!dev->device_cfg ||
        dev->device_cfg_length <= BALLOON_CFG_POISON_VAL_OFFSET + 3)
        return TEST_SKIP;

    uint32_t poison_val = *(volatile uint32_t *)
        ((char *)dev->device_cfg + BALLOON_CFG_POISON_VAL_OFFSET);

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

    uint32_t *page = vv_alloc_pages(1);
    for (unsigned i = 0; i < 4096 / sizeof(uint32_t); i++)
        page[i] = FILL_PATTERN;

    vring_raw_set_desc(&rq, 0, vv_virt_to_phys(page), 4096, 0, 0);
    vring_raw_set_avail(&rq, 0, 0);
    vring_raw_set_avail_idx(&rq, 1);

    test_result_t r = vv_kick_and_wait(dev, &rq, 3, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    __sync_synchronize();
    for (unsigned i = 0; i < 4096 / sizeof(uint32_t); i++)
        if (page[i] != FILL_PATTERN && page[i] != poison_val)
            TFAIL("word %u is 0x%08x, not the pattern or poison_val 0x%08x",
                  i, page[i], poison_val);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(L0045, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_report_poison,
              "reported page keeps the pattern or the poison value",
              VIRTIO_SPEC_V1_2, "5.5.6.4",
              (1ULL << VIRTIO_BALLOON_F_REPORTING) |
              (1ULL << VIRTIO_BALLOON_F_PAGE_POISON), 0);
