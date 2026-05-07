/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0054: device_feature_select past supported pages reads zero
 *
 * Spec 4.1.4.3 says device_feature_select picks a 32 bit window
 * into the feature space. Selectors that index past the last
 * defined feature must return zero in device_feature, and
 * subsequent selects back to 0 must continue to return the same
 * supported set. Walk selectors 2 through 7 and verify zero, then
 * read selector 0 again and verify it matches what was read first.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_dev_feat_past_pages(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t f0a = cfg->device_feature;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t f1a = cfg->device_feature;
    (void)f1a;

    for (uint32_t s = 2; s < 8; s++) {
        cfg->device_feature_select = s;
        __sync_synchronize();
        if (cfg->device_feature != 0)
            TFAIL("cfg->device_feature != 0");
    }

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (cfg->device_feature != f0a)
        TFAIL("cfg->device_feature != f0a");

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0054, VIRTIO_PCI_DEVICE_BLK, test_dev_feat_past_pages,
              "device_feature reads zero for selectors past page 1",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
