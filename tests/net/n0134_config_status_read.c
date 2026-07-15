/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0134: read net status field and verify LINK_UP.
 *
 * Spec 5.1.4: When VIRTIO_NET_F_STATUS is negotiated the device
 * config contains a status field at offset 6. The LINK_UP bit
 * (bit 0) indicates the physical link is active. Read it and
 * verify the field is accessible and the value is within the
 * defined bit mask (bits 0 and 1 only).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_net_config_status(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_STATUS)))
        return TEST_SKIP;

    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;

    volatile uint16_t *status = (volatile uint16_t *)
        ((char *)dev->device_cfg + VIRTIO_NET_CFG_STATUS_OFFSET);

    uint16_t val = *status;

    /* Only bits 0 (LINK_UP) and 1 (ANNOUNCE) are defined */
    if (val & ~(VIRTIO_NET_S_LINK_UP | VIRTIO_NET_S_ANNOUNCE))
        TFAIL("status 0x%04x has undefined bits set", val);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0134, VIRTIO_PCI_DEVICE_NET, test_net_config_status,
              "Read net status field and verify defined bits only",
              VIRTIO_SPEC_V1_2, "5.1.4",
              (1ULL << VIRTIO_NET_F_STATUS), 0);
