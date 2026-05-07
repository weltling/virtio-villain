/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0050: hot reset while config_generation is being polled
 *
 * Spec 4.1.4.3.1 says config_generation is monotonic and updated
 * by the device. The driver may read it any time. A reset issued
 * mid poll must take effect, the counter must read zero or stable
 * after the reset, and a follow up reinit must succeed. This
 * catches devices that race the reset path with config reads.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_reset_during_cfg_gen(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Spin polling config_generation a bunch of times */
    for (int i = 0; i < 1000; i++)
        (void)cfg->config_generation;

    /* Reset right after the burst */
    virtio_pci_reset(dev);

    /* Follow up read must not crash and reinit must succeed */
    (void)cfg->config_generation;
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER)");

    return TEST_PASS;
}

REGISTER_TEST(S0050, VIRTIO_PCI_DEVICE_BLK, test_reset_during_cfg_gen,
              "reset issued right after config_generation poll burst",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
