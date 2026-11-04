/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/sound_util.h"

static test_result_t test_prepare_before_params(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_snd_pcm_info info;
    VV_TRY(snd_get_pcm_info(dev, vr, 0, &info));

    return snd_expect_pcm_command_safe(
        dev, vr, 0, VIRTIO_SND_R_PCM_PREPARE);
}

REGISTER_TEST(SND0040, VIRTIO_PCI_DEVICE_SND,
              test_prepare_before_params,
              "Safely handle PCM_PREPARE before parameters",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
