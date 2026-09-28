/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_jack_info_size_overflow(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg || cfg->jacks < 2)
        return TEST_SKIP;

    struct virtio_snd_query_info req = {
        .hdr.code = VIRTIO_SND_R_JACK_INFO,
        .start_id = 0,
        .count = 2,
        .size = 0x80000000u,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0020, VIRTIO_PCI_DEVICE_SND,
              test_jack_info_size_overflow,
              "Bound overflowing JACK_INFO response size",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
