/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_reconfigure_after_release(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    test_result_t result = snd_prepare_stream(dev, vr, 0);
    if (result != TEST_PASS)
        return result;
    result = snd_expect_pcm_command_ok(
        dev, vr, 0, VIRTIO_SND_R_PCM_RELEASE);
    if (result != TEST_PASS)
        return result;
    return snd_set_valid_params(dev, vr, 0);
}

REGISTER_TEST(SND0048, VIRTIO_PCI_DEVICE_SND,
              test_reconfigure_after_release,
              "Reconfigure PCM stream after release",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
