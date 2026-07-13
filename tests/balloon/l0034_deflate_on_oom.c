/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0034: DEFLATE_ON_OOM exposed and deflate path works.
 *
 * v1.4 5.5.4 defines VIRTIO_BALLOON_F_DEFLATE_ON_OOM (bit 2).
 * When negotiated, the driver may deflate without the legacy
 * MUST_TELL_HOST handshake. Inflate two pages, then deflate
 * the same two pages on the deflate vq (idx 1). The device
 * must complete both requests.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_balloon_deflate_on_oom(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BALLOON_F_DEFLATE_ON_OOM)))
        return TEST_SKIP;

    void *pages = vv_alloc_pages(2);
    uint64_t base = vv_virt_to_phys(pages);
    uint32_t *pfns = vv_alloc_pages(1);
    pfns[0] = (uint32_t)((base >> VIRTIO_BALLOON_PFN_SHIFT) + 0);
    pfns[1] = (uint32_t)((base >> VIRTIO_BALLOON_PFN_SHIFT) + 1);

    /* Inflate on default queue (idx 0). */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pfns),
                       2 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Deflate on queue 1. */
    struct vring dvr;
    if (vring_alloc(&dvr, 16) < 0) return TEST_SKIP;
    vring_attach(dev, &dvr, 1);

    vring_raw_set_desc(&dvr, 0, vv_virt_to_phys(pfns),
                       2 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(&dvr, 0, 0);
    vring_raw_set_avail_idx(&dvr, 1);
    return vv_kick_and_wait(dev, &dvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0034, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_deflate_on_oom,
              "Inflate then deflate with DEFLATE_ON_OOM negotiated",
              VIRTIO_SPEC_V1_4, "5.5.4",
              (1ULL << VIRTIO_BALLOON_F_DEFLATE_ON_OOM), 0);
