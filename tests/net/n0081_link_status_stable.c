/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0081: link_status field reads consistent
 *
 * Spec 5.1.4 defines virtio_net_config.status as a 16 bit field
 * with VIRTIO_NET_S_LINK_UP=1 and VIRTIO_NET_S_ANNOUNCE=2 when
 * VIRTIO_NET_F_STATUS is negotiated. Read the field many times
 * with no driver activity and verify every read returns the same
 * value, which indicates the device is not racing internal state
 * onto the config register.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>


static test_result_t test_net_link_status_stable(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (!dev->device_cfg)
        return TEST_SKIP;
    if (dev->device_cfg_length < VIRTIO_NET_CFG_STATUS_OFFSET + 2)
        return TEST_SKIP;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_STATUS)))
        return TEST_SKIP;

    volatile uint16_t *status = (volatile uint16_t *)(
        (uint8_t *)dev->device_cfg + VIRTIO_NET_CFG_STATUS_OFFSET);

    uint16_t first = *status;
    for (int i = 0; i < 5000; i++) {
        if (*status != first)
            TFAIL("*status != first");
    }

    return TEST_PASS;
}

REGISTER_TEST(N0081, VIRTIO_PCI_DEVICE_NET, test_net_link_status_stable,
              "Net link_status reads stable when idle",
              VIRTIO_SPEC_V1_2, "5.1.4");
