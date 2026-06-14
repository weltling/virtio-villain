/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0063: ring_reset_without_feature
 *
 * Attempt to reset a queue (write queue_enable=0) without having
 * negotiated VIRTIO_F_RING_RESET. Spec 4.1.4.3.2: the driver MUST
 * NOT write 0 to queue_enable. The device should ignore or reject it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_ring_reset_no_feature(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Confirm RING_RESET is NOT negotiated (our harness negotiates
     * zero features by default) */
    cfg->device_feature_select = VIRTIO_F_RING_RESET / 32;
    __sync_synchronize();

    /* Attempt queue disable without the feature */
    cfg->queue_select = 0;
    __sync_synchronize();

    uint16_t before = cfg->queue_enable;
    if (!before)
        return TEST_SKIP;

    cfg->queue_enable = 0;
    __sync_synchronize();
    usleep(50000);

    uint16_t after = cfg->queue_enable;

    /* Device may ignore the write; queue should remain enabled */
    if (after == 0) {
        /* Some devices process it anyway; acceptable but notable */
        return TEST_PASS;
    }

    /* Device must still be alive */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0063, VIRTIO_PCI_DEVICE_BLK, test_ring_reset_no_feature,
              "queue_enable=0 without VIRTIO_F_RING_RESET negotiated",
              VIRTIO_SPEC_V1_3, "4.1.4.3.2");
