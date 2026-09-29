/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_lifecycle_stream_isolation(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg || cfg->streams < 2)
        return TEST_SKIP;

    test_result_t result = snd_prepare_stream(dev, vr, 0);
    if (result != TEST_PASS)
        return result;
    result = snd_submit_pcm_command_safe(
        dev, vr, 0, VIRTIO_SND_R_PCM_PREPARE);
    if (result != TEST_PASS)
        return result;

    return snd_complete_lifecycle(dev, vr, 1);
}

REGISTER_TEST(SND0055, VIRTIO_PCI_DEVICE_SND,
              test_lifecycle_stream_isolation,
              "Preserve another stream after invalid lifecycle",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
