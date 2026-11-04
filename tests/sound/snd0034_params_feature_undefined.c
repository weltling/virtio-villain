/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/sound_util.h"

static test_result_t test_params_feature_undefined(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_snd_pcm_set_params params;
    struct virtio_snd_pcm_info info;
    VV_TRY(snd_make_valid_params( dev, vr, 0, &params, &info));
    params.features = 1u << 31;

    return snd_expect_safe_handling(dev, vr, &params, sizeof(params),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0034, VIRTIO_PCI_DEVICE_SND,
              test_params_feature_undefined,
              "Safely handle undefined PCM feature bit",
              VIRTIO_SPEC_V1_2, "5.14.6.6.3");
