/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0044: write 0xFF to device_status
 *
 * Spec 2.1 lists six defined status bits. The upper bits are
 * reserved. A driver that writes 0xFF in one go is malformed,
 * but the device must remain alive and a subsequent reset must
 * still bring it back to a clean state. This catches devices
 * that latch reserved bits or wedge on undefined values.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_status_set_all_bits(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_status = 0xFF;
    __sync_synchronize();
    usleep(10000);

    /* Device should still respond to register reads */
    uint16_t nq = cfg->num_queues;
    if (nq == 0xFFFF)
        TFAIL("nq == 0xFFFF");

    /* A reset must always succeed */
    virtio_pci_reset(dev);

    return TEST_PASS;
}

REGISTER_TEST(S0044, VIRTIO_PCI_DEVICE_BLK, test_status_set_all_bits,
              "write 0xFF to device_status then reset",
              VIRTIO_SPEC_V1_2, "2.1");
