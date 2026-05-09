/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0057: state_driver_ok_and_failed
 *
 * Write DRIVER_OK and FAILED to device_status in a single byte
 * write. Spec 2.1 makes status writes additive, so this is an
 * unusual combination. The device must tolerate the write
 * without crashing and must accept a subsequent reset.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_state_ok_and_failed(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_status = (uint8_t)(VIRTIO_STATUS_DRIVER_OK
                                   | VIRTIO_STATUS_FAILED);
    __sync_synchronize();
    usleep(10000);

    /* Reset and verify the device accepts a clean restart. */
    virtio_pci_reset(dev);
    if (cfg->device_status != 0)
        TFAIL("cfg->device_status != 0");
    return TEST_PASS;
}

REGISTER_TEST(S0057, VIRTIO_PCI_DEVICE_BLK, test_state_ok_and_failed,
              "Write DRIVER_OK and FAILED simultaneously",
              VIRTIO_SPEC_V1_2, "2.1.1");
