/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_params_channels_high(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_snd_pcm_set_params params;
    struct virtio_snd_pcm_info info;
    test_result_t result = snd_make_valid_params(
        dev, vr, 0, &params, &info);
    if (result != TEST_PASS)
        return result;
    if (info.channels_max == UINT8_MAX)
        return TEST_SKIP;
    params.channels = info.channels_max + 1;

    return snd_expect_safe_handling(dev, vr, &params, sizeof(params),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0030, VIRTIO_PCI_DEVICE_SND,
              test_params_channels_high,
              "Safely handle PCM channels above maximum",
              VIRTIO_SPEC_V1_2, "5.14.6.6.3");
