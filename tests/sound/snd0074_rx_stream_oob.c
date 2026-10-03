/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_rx_stream_oob(struct virtio_dev *dev,
                                        struct vring *control_vr,
                                        struct vring *rx_vr)
{
    (void)control_vr;
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg)
        return TEST_SKIP;

    struct virtio_snd_pcm_xfer *xfer = vv_alloc_pages(1);
    uint8_t *response = vv_alloc_pages(1);
    uint16_t used_idx = rx_vr->used->idx;
    xfer->stream_id = cfg->streams;
    memset(response, 0xff, 1032);
    vring_raw_set_desc(rx_vr, 0, vv_virt_to_phys(xfer), sizeof(*xfer),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(rx_vr, 1, vv_virt_to_phys(response), 1032,
                       VRING_DESC_F_WRITE, 0);
    vring_submit(rx_vr, 0);

    return snd_wait_pcm_safe(
        dev, rx_vr, used_idx, 1032,
        (struct virtio_snd_pcm_status *)response, 1032);
}

REGISTER_TEST_CONTROL_Q(SND0074, VIRTIO_PCI_DEVICE_SND,
                        test_rx_stream_oob,
                        "Safely handle RX stream out of range",
                        VIRTIO_SPEC_V1_2, "5.14.6.8", 3);
