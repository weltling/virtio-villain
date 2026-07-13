/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0067: suspend_basic
 *
 * Negotiate VIRTIO_F_SUSPEND, set SUSPEND bit in device_status,
 * and verify the device clears DRIVER_OK. Spec 3.2: the device
 * sets DRIVER_OK to 0 once it has been suspended.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_suspend_basic(struct virtio_dev *dev,
                                        struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Check if SUSPEND is offered */
    cfg->device_feature_select = VIRTIO_F_SUSPEND / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_SUSPEND % 32))))
        return TEST_SKIP;

    /* Set SUSPEND bit */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();

    /* Wait for device to process */
    usleep(200000);

    /* Verify: SUSPEND=1, DRIVER_OK=0 */
    uint8_t new_status = cfg->device_status;
    if (!(new_status & VIRTIO_STATUS_SUSPEND))
        TFAIL("!(new_status & VIRTIO_STATUS_SUSPEND)");
    if (new_status & VIRTIO_STATUS_DRIVER_OK)
        TFAIL("new_status & VIRTIO_STATUS_DRIVER_OK");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(S0067, VIRTIO_PCI_DEVICE_BLK, test_suspend_basic,
              "Set SUSPEND bit and verify DRIVER_OK clears",
              VIRTIO_SPEC_V1_3, "3.2",
              (1ULL << VIRTIO_F_SUSPEND), 0);
