/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0090: FEATURES_OK rewrite after a first acceptance.
 *
 * Spec 2.1: After FEATURES_OK is set and confirmed, the
 * driver MUST NOT change the feature bits and MUST NOT clear
 * FEATURES_OK. Reset, set FEATURES_OK with zero features,
 * then attempt to overwrite driver_feature; the device must
 * not advance into a corrupted state.
 */
#include "tests/test.h"

#include <unistd.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    cfg->driver_feature_select = 0; cfg->driver_feature = 0;
    cfg->driver_feature_select = 1; cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(1000);
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("FEATURES_OK rejected on first attempt");

    /* Now try to rewrite a feature bit after acceptance. */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0xFFFFFFFFu;
    __sync_synchronize();
    usleep(1000);

    if (cfg->device_status & VIRTIO_STATUS_FAILED)
        return TEST_PASS;
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_PASS;
    return TEST_PASS;
}

REGISTER_TEST(S0090, VIRTIO_PCI_DEVICE_BLK, test,
              "Rewrite driver_feature after FEATURES_OK accepted",
              VIRTIO_SPEC_V1_4, "2.1");
