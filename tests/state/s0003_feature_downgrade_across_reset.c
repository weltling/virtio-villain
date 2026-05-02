/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0003: feature_downgrade_across_reset
 *
 * First init: negotiate all device-offered features. Submit I/O that
 * would exercise those features. Then reset and re-init with zero
 * features, but immediately submit an I/O request that assumes the
 * prior feature set is still active (e.g., send a flush request
 * implying writeback mode).
 *
 * This exercises whether the VMM properly clears internal feature state
 * on reset. A VMM that retains stale feature flags may misparse requests
 * or apply wrong semantics.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN    0
#define VIRTIO_BLK_T_FLUSH 4

static test_result_t test_feature_downgrade(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /*
     * Phase 1: Reset and re-init with ALL offered features.
     */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Read and echo back all device features */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat_lo = cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t feat_hi = cfg->device_feature;

    cfg->driver_feature_select = 0;
    cfg->driver_feature = feat_lo;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = feat_hi;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        /* Device rejected all-features; that's fine, skip */
        return TEST_SKIP;
    }

    /* Set up queue and DRIVER_OK */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    usleep(10000);

    /*
     * Phase 2: Reset and re-init with ZERO features.
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

    struct vring vr3;
    vring_alloc(&vr3, 16);
    vring_attach(dev, &vr3, 0);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /*
     * Phase 3: Send a FLUSH as if VIRTIO_BLK_F_FLUSH was still active.
     * With zero features negotiated, flush should not be accepted.
     */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_FLUSH;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(&vr3, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr3, 1, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&vr3, 0, 0);
    vring_raw_set_avail_idx(&vr3, 1);

    return vv_kick_and_wait(dev, &vr3, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0003, VIRTIO_PCI_DEVICE_BLK, test_feature_downgrade,
              "Reset and re-init with fewer features then use old feature",
              VIRTIO_SPEC_V1_2, "3.1.1");
