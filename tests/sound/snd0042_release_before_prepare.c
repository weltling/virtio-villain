/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_release_before_prepare(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    test_result_t result = snd_set_valid_params(dev, vr, 0);
    if (result != TEST_PASS)
        return result;

    return snd_expect_pcm_command_safe(
        dev, vr, 0, VIRTIO_SND_R_PCM_RELEASE);
}

REGISTER_TEST(SND0042, VIRTIO_PCI_DEVICE_SND,
              test_release_before_prepare,
              "Safely handle PCM_RELEASE before preparation",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
