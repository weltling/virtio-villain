/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0059: state_device_feature_select_max
 *
 * Set device_feature_select to UINT32_MAX, read device_feature.
 * Spec 4.1.4.3.1 says reads of an out of range select must be
 * deterministic; in practice the value is zero. The device must
 * not crash and the read must not stall the bus.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_state_dev_feat_max(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0xFFFFFFFF;
    __sync_synchronize();
    volatile uint32_t v = cfg->device_feature;
    (void)v;

    /* Restore */
    cfg->device_feature_select = 0;
    __sync_synchronize();

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(S0059, VIRTIO_PCI_DEVICE_BLK, test_state_dev_feat_max,
              "device_feature_select set to UINT32_MAX",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
