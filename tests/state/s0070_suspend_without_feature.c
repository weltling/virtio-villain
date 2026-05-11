/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0070: suspend_without_feature
 *
 * Set SUSPEND bit without having negotiated VIRTIO_F_SUSPEND.
 * The device should ignore the bit as it is undefined without
 * the feature.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_STATUS_SUSPEND 16

static test_result_t test_suspend_no_feature(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Our harness negotiates zero features, so SUSPEND is not active */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(100000);

    /* Device should not suspend (DRIVER_OK must remain set) */
    uint8_t new_status = cfg->device_status;
    if (new_status == 0)
        TWEDGED("new_status == 0");
    if (!(new_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(new_status & VIRTIO_STATUS_DRIVER_OK)");

    return TEST_PASS;
}

REGISTER_TEST(S0070, VIRTIO_PCI_DEVICE_BLK, test_suspend_no_feature,
              "Set SUSPEND bit without VIRTIO_F_SUSPEND negotiated",
              VIRTIO_SPEC_V1_3, "3.2");
