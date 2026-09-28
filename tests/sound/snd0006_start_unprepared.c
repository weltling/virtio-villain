/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_pcm_start_unprepared_safe(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg || cfg->streams == 0)
        return TEST_SKIP;

    struct virtio_snd_pcm_set_params params = {
        .hdr.hdr.code = VIRTIO_SND_R_PCM_SET_PARAMS,
        .hdr.stream_id = 0,
        .buffer_bytes = 4096,
        .period_bytes = 1024,
        .channels = 1,
        .format = SND_PCM_FMT_S16,
        .rate = SND_PCM_RATE_48000,
    };
    test_result_t result = snd_expect_ok(dev, vr, &params, sizeof(params));
    if (result != TEST_PASS)
        return result;

    struct virtio_snd_pcm_hdr req = {
        .hdr.code = VIRTIO_SND_R_PCM_START,
        .stream_id = 0,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0006, VIRTIO_PCI_DEVICE_SND, test_pcm_start_unprepared_safe,
              "Safely handle PCM_START before preparation",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");
