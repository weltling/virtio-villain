/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_reset_prepared(struct virtio_dev *dev,
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

    result = snd_reset_reinit(dev, control_vr, tx_vr);
    if (result != TEST_PASS)
        return result;
    result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_OUTPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;
    return snd_complete_lifecycle(dev, control_vr, stream_id);
}

REGISTER_TEST_CONTROL_Q(SND0095, VIRTIO_PCI_DEVICE_SND,
                        test_reset_prepared,
                        "Recover a prepared PCM stream after device reset",
                        VIRTIO_SPEC_V1_2, "2.4.2", 2);
