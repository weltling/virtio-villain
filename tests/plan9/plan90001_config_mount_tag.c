/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90001: read the virtio-9p mount tag from device configuration.
 *
 * Device ID 9 defines feature bit 0 and a configuration payload containing
 * a little endian u16 tag length followed by the tag bytes.
 */
#include "lib/virtio_test.h"
#include "lib/virtio_9p.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    static const char expected[] = "vv9p";
    volatile uint8_t *cfg = dev->device_cfg;

    if (!cfg)
        TSKIP("device has no configuration region");
    if (dev->device_cfg_length < 2 + sizeof(expected) - 1)
        TFAIL("configuration region is only %u bytes",
              dev->device_cfg_length);

    uint16_t tag_len = cfg[0] | ((uint16_t)cfg[1] << 8);
    if (tag_len != sizeof(expected) - 1)
        TFAIL("mount tag length=%u, expected %zu",
              tag_len, sizeof(expected) - 1);

    for (uint16_t i = 0; i < tag_len; i++) {
        if (cfg[2 + i] != (uint8_t)expected[i])
            TFAIL("mount tag differs at byte %u", i);
    }

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(PLAN90001, VIRTIO_PCI_DEVICE_9P, test,
                       "Read the configured virtio-9p mount tag",
                       VIRTIO_SPEC_V1_2, "-",
                       1ULL << VIRTIO_9P_F_MOUNT_TAG, 1);
