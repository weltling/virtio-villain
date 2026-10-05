/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_rx_header_writable(struct virtio_dev *dev,
                                             struct vring *control_vr,
                                             struct vring *rx_vr)
{
    struct virtio_snd_pcm_info info;
    uint32_t stream_id;
    test_result_t result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_INPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;

    struct virtio_snd_pcm_xfer *xfer = vv_alloc_pages(1);
    uint8_t *response = vv_alloc_pages(1);
    uint16_t used_idx = rx_vr->used->idx;
    xfer->stream_id = stream_id;
    memset(response, 0xff, 1032);
    vring_raw_set_desc(rx_vr, 0, vv_virt_to_phys(xfer), sizeof(*xfer),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 1);
    vring_raw_set_desc(rx_vr, 1, vv_virt_to_phys(response), 1032,
                       VRING_DESC_F_WRITE, 0);
    vring_submit(rx_vr, 0);

    return snd_wait_pcm_safe(dev, rx_vr, used_idx,
                             sizeof(*xfer) + 1032,
                             (struct virtio_snd_pcm_status *)xfer,
                             sizeof(*xfer) + 1032);
}

REGISTER_TEST_CONTROL_Q(SND0087, VIRTIO_PCI_DEVICE_SND,
                        test_rx_header_writable,
                        "Safely handle writable RX header",
                        VIRTIO_SPEC_V1_2, "2.7.5", 3);
