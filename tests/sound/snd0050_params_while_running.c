/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_params_while_running(struct virtio_dev *dev,
                                               struct vring *vr)
{
    test_result_t result = snd_start_stream(dev, vr, 0);
    if (result != TEST_PASS)
        return result;

    struct virtio_snd_pcm_set_params params;
    struct virtio_snd_pcm_info info;
    result = snd_make_valid_params(dev, vr, 0, &params, &info);
    if (result != TEST_PASS)
        return result;
    return snd_expect_safe_handling(dev, vr, &params, sizeof(params),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0050, VIRTIO_PCI_DEVICE_SND,
              test_params_while_running,
              "Safely handle SET_PARAMS while running",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
