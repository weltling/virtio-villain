/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_tx_stream_oob(struct virtio_dev *dev,
                                        struct vring *control_vr,
                                        struct vring *tx_vr)
{
    (void)control_vr;
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg)
        return TEST_SKIP;

    return snd_submit_pcm_xfer_safe(
        dev, tx_vr, cfg->streams, false, 1024);
}

REGISTER_TEST_CONTROL_Q(SND0063, VIRTIO_PCI_DEVICE_SND,
                        test_tx_stream_oob,
                        "Safely handle TX stream out of range",
                        VIRTIO_SPEC_V1_2, "5.14.6.8", 2);
