/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0077: forward_compat_reserved_features
 *
 * Set reserved feature bits (bits 24-37 are reserved per spec)
 * in driver features. Spec v1.3 6: reserved feature bits must
 * be ignored and not cause undefined behavior.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_forward_reserved_features(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Write reserved feature bits in page 0 (bits 24-31) */
    cfg->driver_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature = 0xFF000000;  /* Reserved transport bits */

    /* Write reserved bits in page 1 (bits 33-37) */
    cfg->driver_feature_select = 1;
    __sync_synchronize();
    cfg->driver_feature = 0x0000003E;  /* Bits 33-37 */

    /* Attempt to proceed with FEATURES_OK */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE |
                         VIRTIO_STATUS_DRIVER |
                         VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    /* Just verify the device does not crash */
    (void)cfg->device_status;

    return TEST_PASS;
}

REGISTER_TEST(S0077, VIRTIO_PCI_DEVICE_BLK, test_forward_reserved_features,
              "Set reserved feature bits without crashing",
              VIRTIO_SPEC_V1_3, "6");
