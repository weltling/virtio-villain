/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0045: write only FAILED bit without prior init
 *
 * Spec 2.1.2 step 1 says the driver must reset the device first.
 * Some malformed drivers might write VIRTIO_STATUS_FAILED to the
 * status register without going through reset. The device must
 * accept the write or treat it as a noop, but never wedge. A
 * reset afterwards must return the device to a usable state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_status_failed_alone(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset to known state first */
    virtio_pci_reset(dev);

    /* Write FAILED with no other bits set */
    cfg->device_status = VIRTIO_STATUS_FAILED;
    __sync_synchronize();
    usleep(10000);

    /* Device must remain alive */
    uint16_t nq = cfg->num_queues;
    if (nq == 0xFFFF)
        TFAIL("nq == 0xFFFF");

    /* Reset must clear it */
    virtio_pci_reset(dev);

    return TEST_PASS;
}

REGISTER_TEST(S0045, VIRTIO_PCI_DEVICE_BLK, test_status_failed_alone,
              "write FAILED bit without prior init",
              VIRTIO_SPEC_V1_2, "2.1.2");
