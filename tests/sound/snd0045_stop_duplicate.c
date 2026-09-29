/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_stop_duplicate(struct virtio_dev *dev,
                                         struct vring *vr)
{
    test_result_t result = snd_start_stream(dev, vr, 0);
    if (result != TEST_PASS)
        return result;
    result = snd_expect_pcm_command_ok(
        dev, vr, 0, VIRTIO_SND_R_PCM_STOP);
    if (result != TEST_PASS)
        return result;

    return snd_expect_pcm_command_safe(
        dev, vr, 0, VIRTIO_SND_R_PCM_STOP);
}

REGISTER_TEST(SND0045, VIRTIO_PCI_DEVICE_SND,
              test_stop_duplicate,
              "Safely handle duplicate PCM_STOP",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
