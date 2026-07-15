/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0133: read MAC address from net device config.
 *
 * Spec 5.1.4: When VIRTIO_NET_F_MAC is negotiated the first 6
 * bytes of device config contain the MAC address. Read all 6
 * bytes and verify not all zeros and not all ones (broadcast).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_config_mac(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_MAC)))
        return TEST_SKIP;

    if (!dev->device_cfg || dev->device_cfg_length < 6)
        return TEST_SKIP;

    volatile uint8_t *mac = (volatile uint8_t *)dev->device_cfg;
    uint8_t m[6];
    for (int i = 0; i < 6; i++)
        m[i] = mac[i];

    /* Must not be all zeros */
    uint8_t all_zero = 1, all_ff = 1;
    for (int i = 0; i < 6; i++) {
        if (m[i] != 0x00) all_zero = 0;
        if (m[i] != 0xFF) all_ff = 0;
    }
    if (all_zero)
        TFAIL("MAC is all zeros");
    if (all_ff)
        TFAIL("MAC is broadcast (all FF)");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0133, VIRTIO_PCI_DEVICE_NET, test_net_config_mac,
              "Read MAC address from device config",
              VIRTIO_SPEC_V1_2, "5.1.4",
              (1ULL << VIRTIO_NET_F_MAC), 0);
