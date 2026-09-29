/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_params_stream_oob(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg || cfg->streams == 0)
        return TEST_SKIP;

    struct virtio_snd_pcm_set_params params;
    struct virtio_snd_pcm_info info;
    test_result_t result = snd_make_valid_params(
        dev, vr, 0, &params, &info);
    if (result != TEST_PASS)
        return result;
    params.hdr.stream_id = cfg->streams;

    return snd_expect_safe_handling(dev, vr, &params, sizeof(params),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0028, VIRTIO_PCI_DEVICE_SND,
              test_params_stream_oob,
              "Safely handle SET_PARAMS past stream array",
              VIRTIO_SPEC_V1_2, "5.14.6.6.3");
