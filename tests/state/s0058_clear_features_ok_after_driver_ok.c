/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0058: state_clear_features_ok_after_driver_ok
 *
 * After the harness brought the device to DRIVER_OK, clear the
 * FEATURES_OK bit while DRIVER_OK is set. Spec 2.1 makes status
 * writes additive, so this is an out of spec driver action. The
 * device must tolerate the write without crashing and must
 * accept a subsequent reset to a clean zero state.
 */
#include "lib/virtio_test.h"

static test_result_t test_clear_features_ok(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    uint8_t before = cfg->device_status;
    if (!(before & VIRTIO_STATUS_DRIVER_OK))
        return TEST_SKIP;

    cfg->device_status = (uint8_t)(before & ~VIRTIO_STATUS_FEATURES_OK);
    __sync_synchronize();
    usleep(10000);

    /* Reset and verify the device accepts a clean restart. */
    virtio_pci_reset(dev);
    if (cfg->device_status != 0)
        TFAIL("cfg->device_status != 0");
    return TEST_PASS;
}

REGISTER_TEST_DEV(S0058, VIRTIO_PCI_DEVICE_BLK, test_clear_features_ok,
                  "Clear FEATURES_OK while DRIVER_OK is set",
                  VIRTIO_SPEC_V1_2, "2.1");
