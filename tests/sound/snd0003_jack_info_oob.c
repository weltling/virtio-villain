/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_jack_info_oob_safe(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg)
        return TEST_SKIP;

    struct virtio_snd_query_info req = {
        .hdr.code = VIRTIO_SND_R_JACK_INFO,
        .start_id = cfg->jacks,
        .count = 1,
        .size = SND_JACK_INFO_SIZE,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req),
                                    sizeof(struct virtio_snd_hdr) +
                                    SND_JACK_INFO_SIZE);
}

REGISTER_TEST(SND0003, VIRTIO_PCI_DEVICE_SND, test_jack_info_oob_safe,
              "Safely handle JACK_INFO past the jack array",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
