/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_pcm_info_size_short_safe(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg || cfg->streams == 0)
        return TEST_SKIP;

    struct virtio_snd_query_info req = {
        .hdr.code = VIRTIO_SND_R_PCM_INFO,
        .start_id = 0,
        .count = 1,
        .size = SND_PCM_INFO_SIZE - 1,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req),
                                    sizeof(struct virtio_snd_hdr) +
                                    SND_PCM_INFO_SIZE - 1);
}

REGISTER_TEST(SND0005, VIRTIO_PCI_DEVICE_SND,
              test_pcm_info_size_short_safe,
              "Bound a shortened PCM_INFO response",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
