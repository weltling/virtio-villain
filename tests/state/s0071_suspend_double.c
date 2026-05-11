/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0071: suspend_double
 *
 * Set SUSPEND twice (idempotent). The device must not crash or
 * exhibit undefined behavior. After the first suspend takes effect,
 * writing SUSPEND again should be harmless.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_F_SUSPEND 43
#define VIRTIO_STATUS_SUSPEND 16

static test_result_t test_suspend_double(struct virtio_dev *dev,
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

    /* First suspend */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(200000);

    /* Verify suspended */
    uint8_t st1 = cfg->device_status;
    if (!(st1 & VIRTIO_STATUS_SUSPEND))
        TFAIL("!(st1 & VIRTIO_STATUS_SUSPEND)");

    /* Second suspend (redundant write) */
    cfg->device_status = st1 | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(100000);

    /* Must still be suspended and not crashed */
    uint8_t st2 = cfg->device_status;
    if (st2 == 0)
        TWEDGED("st2 == 0");
    if (!(st2 & VIRTIO_STATUS_SUSPEND))
        TFAIL("!(st2 & VIRTIO_STATUS_SUSPEND)");

    return TEST_PASS;
}

REGISTER_TEST(S0071, VIRTIO_PCI_DEVICE_BLK, test_suspend_double,
              "Double SUSPEND write is idempotent",
              VIRTIO_SPEC_V1_3, "3.2");
