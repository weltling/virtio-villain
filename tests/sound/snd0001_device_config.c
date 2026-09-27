/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SND0001: read the virtio-sound device configuration.
 *
 * Spec 5.14.4: the device configuration exposes the number of jacks,
 * PCM streams, and channel maps. Verify that the complete structure is
 * accessible and that the configured topology is usable.
 */
#include "tests/test.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <stdint.h>

struct virtio_snd_config {
    uint32_t jacks;
    uint32_t streams;
    uint32_t chmaps;
} __attribute__((packed));

static test_result_t test_sound_device_config(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct virtio_snd_config))
        TFAIL("sound config is missing or truncated (%u bytes)",
              dev->device_cfg_length);

    volatile struct virtio_snd_config *cfg =
        (volatile struct virtio_snd_config *)dev->device_cfg;
    uint32_t jacks = cfg->jacks;
    uint32_t streams = cfg->streams;
    uint32_t chmaps = cfg->chmaps;

    if (streams == 0)
        TFAIL("sound config advertises no PCM streams");
    if (streams > 1024 || jacks > 1024 || chmaps > 1024)
        TFAIL("implausible sound config counts: jacks=%u streams=%u chmaps=%u",
              jacks, streams, chmaps);

    return TEST_PASS;
}

REGISTER_TEST(SND0001, VIRTIO_PCI_DEVICE_SND, test_sound_device_config,
              "Read sane jack, stream, and channel-map counts",
              VIRTIO_SPEC_V1_2, "5.14.4");
