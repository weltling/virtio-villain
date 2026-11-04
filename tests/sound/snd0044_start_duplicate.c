/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/sound_util.h"

static test_result_t test_start_duplicate(struct virtio_dev *dev,
                                          struct vring *vr)
{
    VV_TRY(snd_start_stream(dev, vr, 0));

    return snd_expect_pcm_command_safe(
        dev, vr, 0, VIRTIO_SND_R_PCM_START);
}

REGISTER_TEST(SND0044, VIRTIO_PCI_DEVICE_SND,
              test_start_duplicate,
              "Safely handle duplicate PCM_START",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
