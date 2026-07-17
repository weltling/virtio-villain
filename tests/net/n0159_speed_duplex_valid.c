/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0159: net_speed_duplex_config_valid
 *
 * With VIRTIO_NET_F_SPEED_DUPLEX negotiated, read the speed and
 * duplex config fields and verify they hold spec-legal values
 * (spec 5.1.4): speed is 0..0x7fffffff or 0xffffffff (unknown),
 * duplex is 0x00 (half), 0x01 (full), or 0xff (unknown).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_net_speed_duplex_valid(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_SPEED_DUPLEX))
        return TEST_SKIP;
    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct virtio_net_config))
        return TEST_SKIP;

    volatile struct virtio_net_config *ncfg =
        (volatile struct virtio_net_config *)dev->device_cfg;

    uint32_t speed = ncfg->speed;
    uint8_t duplex = ncfg->duplex;

    if (speed > 0x7fffffff && speed != 0xffffffff)
        TFAIL("speed 0x%08x is out of range", speed);
    if (duplex != 0x00 && duplex != 0x01 && duplex != 0xff)
        TFAIL("duplex 0x%02x is not half, full, or unknown", duplex);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(N0159, VIRTIO_PCI_DEVICE_NET, test_net_speed_duplex_valid,
              "speed and duplex config hold spec-legal values",
              VIRTIO_SPEC_V1_4, "5.1.4",
              (1ULL << VIRTIO_NET_F_SPEED_DUPLEX), 0);
