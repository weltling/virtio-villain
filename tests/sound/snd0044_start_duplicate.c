/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_start_duplicate(struct virtio_dev *dev,
                                          struct vring *vr)
{
    test_result_t result = snd_start_stream(dev, vr, 0);
    if (result != TEST_PASS)
        return result;

    return snd_expect_pcm_command_safe(
        dev, vr, 0, VIRTIO_SND_R_PCM_START);
}

REGISTER_TEST(SND0044, VIRTIO_PCI_DEVICE_SND,
              test_start_duplicate,
              "Safely handle duplicate PCM_START",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
