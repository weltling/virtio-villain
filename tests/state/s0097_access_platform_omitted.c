/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0097: Deliberately decline offered ACCESS_PLATFORM.
 *
 * Spec 6.1 recommends accepting ACCESS_PLATFORM. Section 6.2 allows
 * the device to refuse operation if the driver declines it.
 */
#include "lib/virtio_test.h"

static test_result_t test_access_platform_omitted(struct virtio_dev *dev)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_ACCESS_PLATFORM))
        TSKIP("ACCESS_PLATFORM not offered");

    virtio_pci_reset(dev);
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status != 0)
        TWEDGED("device reset did not complete");
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    uint32_t version = virtio_pci_base_features_hi(dev) &
                       (1u << (VIRTIO_F_VERSION_1 - 32));
    for (unsigned w = 0; w < 4; w++) {
        cfg->driver_feature_select = w;
        cfg->driver_feature = w == 1 ? version : 0;
    }
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("device refused negotiation without ACCESS_PLATFORM");

    return TEST_PASS;
}

REGISTER_TEST_DEV_REQUIRES(S0097, VIRTIO_PCI_DEVICE_BLK,
                          test_access_platform_omitted,
                          "Decline offered ACCESS_PLATFORM during negotiation",
                          VIRTIO_SPEC_V1_2, "6.2",
                          (1ULL << VIRTIO_F_ACCESS_PLATFORM), 0);
