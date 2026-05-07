/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0047: device_feature_select readback round trip
 *
 * Spec 4.1.4.3.1 says device_feature_select selects which page of
 * device features the driver reads via device_feature. The select
 * register itself is driver writable and must read back the value
 * the driver wrote. Many VMMs implement the field as state on the
 * device side, so a write read cycle catches truncation or aliasing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_feature_select_roundtrip(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    static const uint32_t vals[] = {0, 1, 2, 7, 0x1234, 0xdeadbeef};

    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        cfg->device_feature_select = vals[i];
        __sync_synchronize();
        uint32_t v = cfg->device_feature_select;
        if (v != vals[i])
            TFAIL("v != vals[i]");
    }

    /* Same for driver side */
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        cfg->driver_feature_select = vals[i];
        __sync_synchronize();
        uint32_t v = cfg->driver_feature_select;
        if (v != vals[i])
            TFAIL("v != vals[i]");
    }

    return TEST_PASS;
}

REGISTER_TEST(S0047, VIRTIO_PCI_DEVICE_BLK, test_feature_select_roundtrip,
              "feature_select registers round trip arbitrary values",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
