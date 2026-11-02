/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0136: read MTU from net device config.
 *
 * Spec 5.1.4: When VIRTIO_NET_F_MTU is negotiated the device config
 * contains mtu at offset 10 (after mac[6]+status[2]+max_vq_pairs[2]).
 * Read it and verify it is in a sane range (68..65535).
 */
#include "lib/virtio_test.h"

#define NET_CFG_MTU_OFFSET 10

static test_result_t test_net_config_mtu(struct virtio_dev *dev)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_MTU))
        return TEST_SKIP;

    if (!dev->device_cfg || dev->device_cfg_length <= NET_CFG_MTU_OFFSET + 1)
        return TEST_SKIP;

    volatile uint16_t *mtu = (volatile uint16_t *)
        ((char *)dev->device_cfg + NET_CFG_MTU_OFFSET);

    uint16_t val = *mtu;

    /* IPv4 minimum MTU is 68, max is 65535 */
    if (val < 68)
        TFAIL("mtu %u is below minimum (68)", val);

    return TEST_PASS;
}

REGISTER_TEST_DEV_REQUIRES(N0136, VIRTIO_PCI_DEVICE_NET, test_net_config_mtu,
                  "Read MTU from device config and validate range",
                  VIRTIO_SPEC_V1_2, "5.1.4",
                  (1ULL << VIRTIO_NET_F_MTU), 0);
