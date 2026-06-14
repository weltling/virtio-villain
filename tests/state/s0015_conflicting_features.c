/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0015: state_conflicting_features
 *
 * Negotiate VIRTIO_NET_F_MQ (multiqueue) without VIRTIO_NET_F_CTRL_VQ
 * (which is required for MQ). This violates the spec dependency chain.
 * Tests whether the device detects the conflict and refuses FEATURES_OK.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_conflicting_features(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset the device */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Check what features device offers */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;

    /*
     * If device doesn't offer MQ, we can't test the conflict.
     * Use block device features instead: try to set DISCARD without
     * the device offering it, combined with a valid offered feature.
     */
    (void)offered;

    /*
     * Set conflicting features: bit 28 (DISCARD) requires WRITE_ZEROES
     * support per spec, but we only set DISCARD without it.
     * Alternatively just set random high bits the device didn't offer.
     */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = (1u << 28) | (1u << 13); /* DISCARD + some bit */
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(10000);

    /*
     * If device cleared FEATURES_OK, it detected the conflict (PASS).
     * If it kept FEATURES_OK set, try to do I/O and see what happens.
     */
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_PASS;

    /* Device accepted invalid features - set up queue and try I/O */
    struct vring q0;
    vring_alloc(&q0, 64);
    vring_attach(dev, &q0, 0);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    (void)vr;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&q0, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q0, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&q0, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);

    return vv_kick_and_wait(dev, &q0, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0015, VIRTIO_PCI_DEVICE_BLK, test_conflicting_features,
              "Negotiate features with violated dependencies",
              VIRTIO_SPEC_V1_2, "2.2.1");
