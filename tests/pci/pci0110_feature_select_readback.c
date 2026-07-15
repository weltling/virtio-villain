/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0110: device_feature_select readback matches.
 *
 * Spec 4.1.4.3: Writing device_feature_select selects the feature
 * word. Reading it back must return the written value.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_fsel_readback(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t rb0 = cfg->device_feature_select;

    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t rb1 = cfg->device_feature_select;

    if (rb0 != 0) TFAIL("feature_select readback %u after writing 0", rb0);
    if (rb1 != 1) TFAIL("feature_select readback %u after writing 1", rb1);

    return TEST_PASS;
}

REGISTER_TEST(PCI0110, VIRTIO_PCI_DEVICE_BLK, test_pci_fsel_readback,
              "device_feature_select readback matches written value",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
