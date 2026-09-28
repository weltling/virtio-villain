/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_pcm_info_count_zero(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_snd_query_info req = {
        .hdr.code = VIRTIO_SND_R_PCM_INFO,
        .start_id = 0,
        .count = 0,
        .size = SND_PCM_INFO_SIZE,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0013, VIRTIO_PCI_DEVICE_SND,
              test_pcm_info_count_zero,
              "Safely handle zero-count PCM_INFO query",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
