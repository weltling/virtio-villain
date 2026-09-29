/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_params_feature_unsupported(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_snd_pcm_set_params params;
    struct virtio_snd_pcm_info info;
    test_result_t result = snd_make_valid_params(
        dev, vr, 0, &params, &info);
    if (result != TEST_PASS)
        return result;

    uint32_t feature = 1;
    while (feature && (info.features & feature))
        feature <<= 1;
    if (!feature || feature & ~SND_PCM_FEATURES_VALID)
        return TEST_SKIP;
    params.features = feature;

    return snd_expect_safe_handling(dev, vr, &params, sizeof(params),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0033, VIRTIO_PCI_DEVICE_SND,
              test_params_feature_unsupported,
              "Safely handle unsupported PCM feature",
              VIRTIO_SPEC_V1_2, "5.14.6.6.3");
