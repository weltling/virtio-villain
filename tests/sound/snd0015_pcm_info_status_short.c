/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_pcm_info_status_short(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_snd_query_info req = {
        .hdr.code = VIRTIO_SND_R_PCM_INFO,
        .start_id = 0,
        .count = 0,
        .size = SND_PCM_INFO_SIZE,
    };
    uint32_t status = 0;
    uint32_t used_len = 0;
    const uint32_t response_len = sizeof(struct virtio_snd_hdr) - 1;
    test_result_t result = snd_submit_control(
        dev, vr, &req, sizeof(req), response_len, &status, &used_len);
    if (result != TEST_PASS)
        return result;
    if (used_len > response_len)
        TFAIL("sound response length %u exceeds writable buffer %u",
              used_len, response_len);
    return TEST_PASS;
}

REGISTER_TEST(SND0015, VIRTIO_PCI_DEVICE_SND,
              test_pcm_info_status_short,
              "Bound PCM_INFO response shorter than status",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
