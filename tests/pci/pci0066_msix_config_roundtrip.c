/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0066: msix_config readback returns written value or NO_VECTOR
 *
 * Spec 4.1.4.3.1 says msix_config is the device wide MSI-X vector
 * for config interrupts. The driver writes a vector and reads it
 * back. If the write succeeds the readback equals the value, if
 * the device cannot honour the request the readback equals the
 * special VIRTIO_MSI_NO_VECTOR sentinel. Any other value is a
 * spec violation.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_MSI_NO_VECTOR 0xFFFF

static test_result_t test_pci_msix_config_roundtrip(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset so the round trip happens on a quiescent device */
    virtio_pci_reset(dev);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    static const uint16_t vals[] = {0, 1, 2, VIRTIO_MSI_NO_VECTOR};

    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        cfg->msix_config = vals[i];
        __sync_synchronize();
        uint16_t v = cfg->msix_config;
        if (v != vals[i] && v != VIRTIO_MSI_NO_VECTOR)
            TFAIL("v != vals[i] && v != VIRTIO_MSI_NO_VECTOR");
    }

    /* Restore */
    cfg->msix_config = VIRTIO_MSI_NO_VECTOR;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(PCI0066, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_config_roundtrip,
              "msix_config readback equals value or NO_VECTOR",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
