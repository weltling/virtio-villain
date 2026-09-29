/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_lifecycle_truncated(struct virtio_dev *dev,
                                              struct vring *vr)
{
    test_result_t result = snd_set_valid_params(dev, vr, 0);
    if (result != TEST_PASS)
        return result;

    struct virtio_snd_pcm_hdr req = {
        .hdr.code = VIRTIO_SND_R_PCM_PREPARE,
        .stream_id = 0,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req) - 1,
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0053, VIRTIO_PCI_DEVICE_SND,
              test_lifecycle_truncated,
              "Safely handle truncated lifecycle command",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
