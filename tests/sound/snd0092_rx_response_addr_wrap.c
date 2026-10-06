/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_rx_response_addr_wrap(struct virtio_dev *dev,
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

    struct virtio_snd_pcm_xfer *xfer = vv_alloc_pages(1);
    uint16_t used_idx = rx_vr->used->idx;
    xfer->stream_id = stream_id;
    vring_raw_set_desc(rx_vr, 0, vv_virt_to_phys(xfer), sizeof(*xfer),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(rx_vr, 1, UINT64_MAX - 511, 1032,
                       VRING_DESC_F_WRITE, 0);
    vring_submit(rx_vr, 0);

    return snd_wait_pcm_safe(dev, rx_vr, used_idx, 1032, NULL, 0);
}

REGISTER_TEST_CONTROL_Q(SND0092, VIRTIO_PCI_DEVICE_SND,
                        test_rx_response_addr_wrap,
                        "Safely handle wrapping RX response range",
                        VIRTIO_SPEC_V1_2, "2.7.5", 3);
