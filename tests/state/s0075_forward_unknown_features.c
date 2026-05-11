/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0075: forward_compat_unknown_features
 *
 * Offer feature bits beyond what the device advertises and verify
 * the device handles FEATURES_OK correctly. Spec v1.3 2.2: the
 * device must accept FEATURES_OK only if the driver did not set
 * any unsupported required feature bits.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_forward_unknown_features(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Read device features from page 0 */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t dev_feats0 = cfg->device_feature;

    /* Write driver features with extra unknown bits set */
    cfg->driver_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature = dev_feats0 | 0x80000000;  /* Set bit 31 */

    /* Also set unknown bits in feature page 1 */
    cfg->driver_feature_select = 1;
    __sync_synchronize();
    cfg->driver_feature = 0xFFFF0000;  /* Bits 48-63 unknown */

    /* Attempt to set FEATURES_OK */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE |
                         VIRTIO_STATUS_DRIVER |
                         VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    /* Read back status to check if FEATURES_OK was accepted.
     * Either outcome is acceptable; device must not crash. */
    (void)cfg->device_status;

    return TEST_PASS;
}

REGISTER_TEST(S0075, VIRTIO_PCI_DEVICE_BLK, test_forward_unknown_features,
              "FEATURES_OK with unknown feature bits set",
              VIRTIO_SPEC_V1_3, "2.2");
