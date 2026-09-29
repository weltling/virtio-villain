/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_params_rate_unsupported(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_snd_pcm_set_params params;
    struct virtio_snd_pcm_info info;
    test_result_t result = snd_make_valid_params(
        dev, vr, 0, &params, &info);
    if (result != TEST_PASS)
        return result;

    uint8_t rate = 0;
    while (rate < 64 && (info.rates & (1ULL << rate)))
        rate++;
    params.rate = rate < 64 ? rate : UINT8_MAX;

    return snd_expect_safe_handling(dev, vr, &params, sizeof(params),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0032, VIRTIO_PCI_DEVICE_SND,
              test_params_rate_unsupported,
              "Safely handle unsupported PCM rate",
              VIRTIO_SPEC_V1_2, "5.14.6.6.3");
