/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0031: Emergency write without negotiating the feature.
 *
 * Spec 5.3.4: If VIRTIO_CONSOLE_F_EMERG_WRITE is not negotiated,
 * the config register at offset 12 may not exist. Writing to it
 * anyway must not crash the device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_CONSOLE_F_EMERG_WRITE 2

static test_result_t test_console_emerg_no_feat(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (dev->device_cfg == NULL)
        return TEST_SKIP;

    /* Confirm feature not offered, or skip if it is */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (feat & (1u << VIRTIO_CONSOLE_F_EMERG_WRITE))
        return TEST_SKIP;

    /* Write anyway; must not crash */
    if (dev->device_cfg_length >= 16) {
        volatile uint32_t *emerg = (volatile uint32_t *)
            ((char *)dev->device_cfg + 12);
        *emerg = (uint32_t)'X';
        __sync_synchronize();
    }

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(C0031, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_emerg_no_feat,
              "Emergency write without feature negotiated",
              VIRTIO_SPEC_V1_3, "5.3.4");
