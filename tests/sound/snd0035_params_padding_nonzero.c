/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/sound_util.h"

static test_result_t test_params_padding_nonzero(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_snd_pcm_set_params params;
    struct virtio_snd_pcm_info info;
    VV_TRY(snd_make_valid_params( dev, vr, 0, &params, &info));
    params.padding = 0xff;

    return snd_expect_safe_handling(dev, vr, &params, sizeof(params),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0035, VIRTIO_PCI_DEVICE_SND,
              test_params_padding_nonzero,
              "Safely handle nonzero SET_PARAMS padding",
              VIRTIO_SPEC_V1_2, "5.14.6.6.3");
