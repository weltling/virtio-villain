/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_rx_valid(struct virtio_dev *dev,
                                   struct vring *control_vr,
                                   struct vring *rx_vr)
{
    struct virtio_snd_pcm_info info;
    uint32_t stream_id;
    test_result_t result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_INPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;
    result = snd_start_stream(dev, control_vr, stream_id);
    if (result != TEST_PASS)
        return result;

    return snd_submit_pcm_xfer(dev, rx_vr, stream_id, true, 1024);
}

REGISTER_TEST_CONTROL_Q(SND0057, VIRTIO_PCI_DEVICE_SND,
                        test_rx_valid,
                        "Complete a valid PCM capture transfer",
                        VIRTIO_SPEC_V1_2, "5.14.6.8", 3);
