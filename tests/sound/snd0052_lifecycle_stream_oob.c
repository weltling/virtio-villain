/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_lifecycle_stream_oob(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg)
        return TEST_SKIP;

    return snd_expect_pcm_command_safe(
        dev, vr, cfg->streams, VIRTIO_SND_R_PCM_PREPARE);
}

REGISTER_TEST(SND0052, VIRTIO_PCI_DEVICE_SND,
              test_lifecycle_stream_oob,
              "Safely handle lifecycle stream out of range",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
