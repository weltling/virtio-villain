/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_pcm_info_size_huge(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg || cfg->streams == 0)
        return TEST_SKIP;

    struct virtio_snd_query_info req = {
        .hdr.code = VIRTIO_SND_R_PCM_INFO,
        .start_id = 0,
        .count = 1,
        .size = UINT32_MAX,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0014, VIRTIO_PCI_DEVICE_SND,
              test_pcm_info_size_huge,
              "Safely handle huge PCM_INFO element size",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
