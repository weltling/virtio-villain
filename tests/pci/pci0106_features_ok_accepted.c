/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0106: FEATURES_OK accepted when valid features written.
 *
 * Spec 3.1.1 step 5: After writing valid driver features, set
 * FEATURES_OK and re-read status to confirm the bit stuck. This
 * tests the fundamental feature negotiation acceptance path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_features_ok(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Accept all device features */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0 = cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t f1 = cfg->device_feature;

    cfg->driver_feature_select = 0;
    cfg->driver_feature = f0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = f1;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    uint8_t st = cfg->device_status;
    if (!(st & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("FEATURES_OK did not stick (status 0x%02x)", st);

    /* Clean up */
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(PCI0106, VIRTIO_PCI_DEVICE_BLK, test_pci_features_ok,
              "FEATURES_OK accepted for valid feature set",
              VIRTIO_SPEC_V1_2, "3.1.1");
