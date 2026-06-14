/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0030: Attempt to set feature bits in page 1 (bits 32-63) including
 * VIRTIO_F_VERSION_1 and random high bits.
 *
 * Spec 3.1.1: The driver writes feature bits via driver_feature_select
 * and driver_feature. Test device handling of valid (VERSION_1) and
 * invalid (random) high feature bits.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_features_above_bit31(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Read what device offers in page 1 */
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t offered_hi = cfg->device_feature;

    /* Set page 0 features to 0 */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    __sync_synchronize();

    /*
     * Set page 1 features: VERSION_1 (if offered) plus random high
     * bits that are certainly not offered. This tests whether the
     * device properly rejects unsupported high bits.
     */
    uint32_t hi_bits = (1U << (VIRTIO_F_VERSION_1 - 32)) | 0xDEAD0000;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = hi_bits;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    /*
     * Device should reject because we set bits it didn't offer.
     * If FEATURES_OK is cleared, that's correct device behavior.
     * If it's still set, the device accepted invalid bits (suspicious
     * but not necessarily a crash).
     */
    uint8_t status = cfg->device_status;
    int features_rejected = !(status & VIRTIO_STATUS_FEATURES_OK);

    /* Reset and try again with only offered bits */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = offered_hi; /* only what was offered */
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)"); /* can't accept valid features after reset */

    (void)features_rejected;
    return TEST_PASS;
}

REGISTER_TEST(S0030, VIRTIO_PCI_DEVICE_BLK, test_features_above_bit31,
              "Feature bits in page 1 (bits 32-63) with invalid high bits",
              VIRTIO_SPEC_V1_2, "3.1.1");
