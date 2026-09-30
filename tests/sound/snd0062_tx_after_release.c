/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_tx_after_release(struct virtio_dev *dev,
                                           struct vring *control_vr,
                                           struct vring *tx_vr)
{
    struct virtio_snd_pcm_info info;
    uint32_t stream_id;
    test_result_t result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_OUTPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;
    result = snd_prepare_stream(dev, control_vr, stream_id);
    if (result != TEST_PASS)
        return result;
    result = snd_expect_pcm_command_ok(
        dev, control_vr, stream_id, VIRTIO_SND_R_PCM_RELEASE);
    if (result != TEST_PASS)
        return result;

    return snd_submit_pcm_xfer_safe(dev, tx_vr, stream_id, false, 1024);
}

REGISTER_TEST_CONTROL_Q(SND0062, VIRTIO_PCI_DEVICE_SND,
                        test_tx_after_release,
                        "Safely handle TX after PCM_RELEASE",
                        VIRTIO_SPEC_V1_2, "5.14.6.8", 2);
