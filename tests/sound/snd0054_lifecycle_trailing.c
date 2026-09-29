/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

struct lifecycle_request_trailing {
    struct virtio_snd_pcm_hdr hdr;
    uint64_t trailing;
} __attribute__((packed));

static test_result_t test_lifecycle_trailing(struct virtio_dev *dev,
                                             struct vring *vr)
{
    test_result_t result = snd_set_valid_params(dev, vr, 0);
    if (result != TEST_PASS)
        return result;

    struct lifecycle_request_trailing req = {
        .hdr.hdr.code = VIRTIO_SND_R_PCM_PREPARE,
        .hdr.stream_id = 0,
        .trailing = UINT64_MAX,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0054, VIRTIO_PCI_DEVICE_SND,
              test_lifecycle_trailing,
              "Safely handle lifecycle trailing bytes",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
