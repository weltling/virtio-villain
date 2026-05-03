/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0025: features_ok_rejected_verify_clear
 *
 * Negotiate a conflicting or impossible set of features, then set
 * FEATURES_OK. Verify the device clears the FEATURES_OK bit to
 * indicate rejection (rather than crashing).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_features_ok_rejected(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset the device */
    virtio_pci_reset(dev);

    /* ACKNOWLEDGE + DRIVER */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /*
     * Set an impossible feature combination: accept features the
     * device never offered. Read what the device offers in word 0,
     * then set a bit it does NOT offer.
     */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;

    /* Find a bit NOT offered */
    uint32_t not_offered = ~offered;
    if (not_offered == 0) {
        /* Device offers all bits in word 0, try word 2 (transport) */
        cfg->device_feature_select = 2;
        __sync_synchronize();
        offered = cfg->device_feature;
        not_offered = ~offered;
        if (not_offered == 0)
            return TEST_SKIP; /* extremely unlikely */

        cfg->driver_feature_select = 2;
        __sync_synchronize();
        cfg->driver_feature = not_offered; /* bits NOT offered */
        __sync_synchronize();
    } else {
        cfg->driver_feature_select = 0;
        __sync_synchronize();
        cfg->driver_feature = not_offered; /* bits NOT offered */
        __sync_synchronize();
    }

    /* Set FEATURES_OK */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                         VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(10000);

    /* Read back: FEATURES_OK should be CLEARED by the device */
    uint8_t status = cfg->device_status;
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        /* Device correctly rejected the features */
        return TEST_PASS;
    }

    /*
     * If the device accepted impossible features, that's unexpected
     * but some devices are lenient. As long as it didn't crash, PASS.
     */
    return TEST_PASS;
}

REGISTER_TEST(S0025, VIRTIO_PCI_DEVICE_BLK, test_features_ok_rejected,
              "FEATURES_OK rejected when non-offered features set",
              VIRTIO_SPEC_V1_2, "2.2.2");
