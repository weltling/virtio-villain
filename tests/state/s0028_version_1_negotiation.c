/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0028: Feature negotiation with VIRTIO_F_VERSION_1 (spec 6)
 *
 * Modern virtio requires VIRTIO_F_VERSION_1 (bit 32) to be
 * negotiated. Verify the device offers it and accepts it.
 * Also try NOT accepting it to test transitional behavior.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_F_VERSION_1 32

static test_result_t test_version_1_negotiation(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset */
    virtio_pci_reset(dev);

    /* Check device offers VERSION_1 */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 1; /* bits 32-63 */
    __sync_synchronize();
    uint32_t offered_hi = cfg->device_feature;

    if (!(offered_hi & (1U << (VIRTIO_F_VERSION_1 - 32))))
        TFAIL("!(offered_hi & (1U << (VIRTIO_F_VERSION_1 - 32)))"); /* modern device MUST offer VERSION_1 */

    /* Accept VERSION_1 */
    cfg->driver_feature_select = 1;
    __sync_synchronize();
    cfg->driver_feature = (1U << (VIRTIO_F_VERSION_1 - 32));
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)"); /* device must accept VERSION_1 */

    /* Now test NOT accepting VERSION_1 */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Accept nothing */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    /*
     * Device MAY reject (clear FEATURES_OK) if VERSION_1 is required.
     * Either outcome is valid for a modern-only device.
     */
    uint8_t status = cfg->device_status;
    (void)status;

    /* Reset and leave device clean */
    cfg->device_status = 0;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(S0028, VIRTIO_PCI_DEVICE_BLK, test_version_1_negotiation,
              "VIRTIO_F_VERSION_1 offered and negotiation behavior",
              VIRTIO_SPEC_V1_2, "6");
